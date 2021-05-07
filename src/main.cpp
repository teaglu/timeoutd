#include "system.h"

#include "Entry.h"
#include "Scheduler.h"

#include "Listener.h"
#include "UdpListener.h"
#include "SimpleListener.h"
#include "SignedListener.h"

#include "Sender.h"
#include "UdpSender.h"
#include "SimpleSender.h"
#include "SignedSender.h"

#include "Multicast.h"

#include "protocol.h"

#include "Log.h"

#include "OpensslMagic.h"

static volatile bool rundown;

static void doExit(int junk)
{
	Log::log(LOG_INFO, "Queueing exit because of signal");
	rundown= true;
}

static void addSender(
	std::list<std::string>& preSharedKeys,
	int senderFrequency,
	char const *senderKey,
	int senderTimeout,
	std::list<SenderRef>& senders,
	char const *peer)
{
	if (preSharedKeys.empty()) {
		SenderRef sender= SimpleSender::Create(peer,
			KEEPALIVE_SIMPLE_PORT, senderFrequency,
			senderKey, senderTimeout);

		if (!sender) {
			Log::log(LOG_ERROR, "Unable to parse sender %s", peer);
			exit(1);
		}
		senders.push_back(sender);
	} else {
		std::string firstKey= preSharedKeys.front();

		SenderRef sender= SignedSender::Create(peer,
			KEEPALIVE_SIGNED_PORT, senderFrequency,
			firstKey.c_str(),
			senderKey, senderTimeout);

		if (!sender) {
			Log::log(LOG_ERROR, "Unable to parse sender %s", peer);
			exit(1);
		}
		senders.push_back(sender);
	}
}

static void addSenderFile(
	std::list<std::string>& preSharedKeys,
	int senderFrequency,
	char const *senderKey,
	int senderTimeout,
	std::list<SenderRef>& senders,
	char const *path)
{
	Log::log(LOG_DEBUG,
		"Reading peers from list file %s", path);

	FILE *file= fopen(path, "rt");
	if (file == NULL) {
		Log::log(LOG_ERROR,
			"Unable to read peer file %s: %s",
			path, strerror(errno));
		exit(1);
	}

	char line[256];
	while (fgets(line, 255, file) != NULL) {
		char *comment= strchr(line, '#');
		if (comment != NULL) {
			*comment= '\0';
		}
		comment= strchr(line, ';');
		if (comment != NULL) {
			*comment= '\0';
		}

		for (char *c= &line[strlen(line) - 1];
			(c >= line) && (*c <= ' '); c--) *c= '\0';

		if (line[0] != '\0') {
			addSender(
				preSharedKeys, senderFrequency, senderKey,
				senderTimeout, senders, line);
		}
	}
	fclose(file);
}

int main(int argc, char* argv[])
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, doExit);
	signal(SIGINT, doExit);

	opensslStartupIncantations();

	int entryLimit= 200;

	std::list<SenderRef> senders;

	int senderFrequency= 2;
	int senderTimeout= 10;

	bool useMulticast= false;
	int multicastTtl= 5;

	std::string senderKey;
	{
		struct utsname nameInfo;
		if (uname(&nameInfo) == -1) {
			Log::log(LOG_ERROR,
				"Unable to read node name: %s",
				strerror(errno));
		}

		// We don't want the FQDN just the node
		char *firstDot= strchr(nameInfo.nodename, '.');
		if (firstDot != NULL) {
			*firstDot= '\0';
		}

		senderKey= "node.";
		senderKey.append(nameInfo.nodename);
	}

	std::list<std::string> preSharedKeys;

	char *envVal;
	if ((envVal= getenv("SIGNATURE_KEY")) != NULL) {
		preSharedKeys.push_back(std::string(envVal));
	}

	int c;
	while ((c= getopt(argc, argv, "F:T:K:p:P:s:M:mvl:")) != -1) {
		switch (c) {
		case 'F':
			senderFrequency= atoi(optarg);
			if (senderFrequency <= 0) {
				Log::log(LOG_ERROR, "Invalid sender frequency");
				exit(1);
			}
			break;

		case 'T':
			senderTimeout= atoi(optarg);
			if (senderTimeout <= 0) {
				Log::log(LOG_ERROR, "Invalid sender timeout");
				exit(1);
			}
			break;

		case 'K':
			senderKey= optarg;
			break;

		case 'M':
			multicastTtl= atoi(optarg);
			if (multicastTtl <= 0) {
				Log::log(LOG_ERROR, "Invalid value for multicast TTL");
				exit(1);
			}
			break;

		case 'm':
			useMulticast= true;
			break;

		case 'l':
			entryLimit= atoi(optarg);
			if (entryLimit < 1) {
				Log::log(LOG_ERROR, "Invalid value for entry limit");
				exit(1);
			}
			break;

		case 'p':
			addSender(
				preSharedKeys, senderFrequency, senderKey.c_str(),
				senderTimeout, senders, optarg);
			break;

		case 'P':
			addSenderFile(
				preSharedKeys, senderFrequency, senderKey.c_str(),
				senderTimeout, senders, optarg);
			break;

		case 's':
			Entry::setNotifyScript(optarg);
			break;

		case 'v':
			Log::setLogLevel(LOG_DEBUG);
			break;
	
		default:
			fprintf(stderr, "Unknown argument %c\n", optopt);
			exit(1);
		}
	}

	if (useMulticast) {
		if (!Multicast::init(multicastTtl)) {
			Log::log(LOG_ERROR, "Multicast bootstrap failed");
			exit(1);
		}
	}

	SchedulerRef scheduler= Scheduler::Create(entryLimit);
	scheduler->start();

	std::list<ListenerRef> listeners;

	listeners.push_back(SimpleListener::Create(
		scheduler, AF_INET, KEEPALIVE_SIMPLE_PORT, useMulticast));
	listeners.push_back(SimpleListener::Create(
		scheduler, AF_INET6, KEEPALIVE_SIMPLE_PORT, false));

	if (!preSharedKeys.empty()) {
		SignedListenerRef listener;

		listener= SignedListener::Create(
			scheduler, AF_INET, KEEPALIVE_SIGNED_PORT, useMulticast);
		for (std::string key : preSharedKeys) {
			listener->addPreSharedKey(key.c_str());
		}
		listeners.push_back(listener);

		listener= SignedListener::Create(
			scheduler, AF_INET6, KEEPALIVE_SIGNED_PORT, false);
		for (std::string key : preSharedKeys) {
			listener->addPreSharedKey(key.c_str());
		}
		listeners.push_back(listener);
	}

	for (ListenerRef listener : listeners) {
		listener->start();
	}

	if (useMulticast) {
		if (preSharedKeys.empty()) {
			SenderRef sender= SimpleSender::Create(MCAST_ADDRESS,
				KEEPALIVE_SIMPLE_PORT, senderFrequency,
				senderKey.c_str(), senderTimeout);

			senders.push_back(sender);
		} else {
			std::string firstKey= preSharedKeys.front();

			SenderRef sender= SignedSender::Create(MCAST_ADDRESS,
				KEEPALIVE_SIGNED_PORT, senderFrequency,
				firstKey.c_str(),
				senderKey.c_str(), senderTimeout);

			senders.push_back(sender);
		}
	}

	for (SenderRef sender : senders) {
		sender->start();	
	}

	Log::log(LOG_INFO, "Server running normally");
	for (rundown= false; !rundown; ) {
		pause();
	}

	Log::log(LOG_DEBUG, "Stopping Senders");
	for (SenderRef sender : senders) {
		sender->stop();
	}
	senders.clear();

	Log::log(LOG_DEBUG, "Stopping Listeners");
	for (ListenerRef listener : listeners) {
		listener->stop();
	}
	listeners.clear();

	Log::log(LOG_DEBUG, "Stopping Scheduler");
	scheduler->stop();

	Log::log(LOG_INFO, "Normal shutdown");

	opensslShutdownIncantations();
}

