#include "system.h"

#include "Entry.h"
#include "Log.h"

std::string Entry::notifyScript= SCRIPTDIR "/timeoutd-notify";

Entry::Entry(char const *key, struct timeval & expires, char const *address)
{
	this->key= key;
	this->expires= expires;
	this->lastAddress= address;
}

Entry::~Entry()
{
}

void Entry::notify()
{
	Log::log(LOG_INFO,
		"Timeout for %s (%s)",
		key.c_str(), lastAddress.c_str());

	char const *childArgv[4];
	childArgv[0]= notifyScript.c_str();
	childArgv[1]= key.c_str();
	childArgv[2]= lastAddress.c_str();
	childArgv[3]= NULL;

	pid_t childPid= fork();
	if (childPid == -1) {
		Log::log(LOG_ERROR,
			"Failed to fork for notification: %s",
			strerror(errno));
	} else if (childPid == 0) {
		// This is the child

		execv(childArgv[0], (char **)childArgv);

		Log::log(LOG_ERROR, "Failed to launch notify script %s: %s",
			childArgv[0], strerror(errno));

		_exit(99);
	} else {
		for (bool wait= true; wait; ) {
			wait=false;

			int status;
			pid_t waitRval= waitpid(childPid, &status, 0);
			if (waitRval == -1) {
				if (errno == EINTR) {
					wait= true;
				} else {
					Log::log(LOG_ERROR,
						"Error waiting for notification script: %s",
						strerror(errno));
				}
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					Log::log(LOG_WARNING,
						"Notification script exited with status %d",
						WEXITSTATUS(status));
				}
			} else if (WIFSIGNALED(status)) {
				Log::log(LOG_WARNING,
					"Notification script exited on signal %d",
					WTERMSIG(status));
			}
		}
	}
}

bool Entry::CompareTimeout(
	const std::shared_ptr<Entry> a,
	const std::shared_ptr<Entry> b)
{
	struct timeval *aExpires= a->getExpires();
	struct timeval *bExpires= b->getExpires();

	if (aExpires->tv_sec < bExpires->tv_sec) {
		return true;
	} else if (aExpires->tv_sec > bExpires->tv_sec) {
		return false;
	} else {
		if (aExpires->tv_usec < bExpires->tv_usec) {
			return true;
		} else if (aExpires->tv_usec > bExpires->tv_usec) {
			return false;
		} else {
			// Because we're using a multiset instead of a
			// priority queue, we have to make sure that
			// our key is absolutely unique or we could
			// remove the wrong item.
			return (strcmp(a->getKey(), b->getKey()) < 0);
		}
	}
}

