#include "system.h"

#include "Log.h"
#include "Entry.h"
#include "Worker.h"
#include "Scheduler.h"

// #define DEBUG_RECEIVED
// #define DEBUG_SCHEDLOOP

Scheduler::Scheduler(int entryLimit)
{
	this->entryLimit= entryLimit;
	entryCount= 0;
}

Scheduler::~Scheduler()
{
}

void Scheduler::receive(char const *key, int timeout, char const *address)
{
	struct timeval expires;
	gettimeofday(&expires, NULL);
	expires.tv_sec+= timeout;

	std::lock_guard<std::mutex> lock(mutex);

	auto keyIter= byKey.find(key);
	EntryRef entry;

	if (timeout <= 0) {
		// A caller can send a 0 timeout to indicate that we should
		// stop monitoring without notification.

		if (keyIter != byKey.end()) {
			byKey.erase(keyIter);
			entryCount--;

			entry= keyIter->second;
			byTimeout.erase(entry);

			Log::log(LOG_INFO,
				"Volutary removal of key %s",
				key);
		}
	} else {
		if (keyIter == byKey.end()) {
			if (entryCount >= entryLimit) {
				Log::log(LOG_WARNING,
					"Cannot add entry - entry count %d is at quota",
					entryLimit);

			} else {
				entry= std::make_shared<Entry>(key, expires, address);
				byKey.insert(
					std::pair<char const *, EntryRef>(entry->getKey(), entry));
				entryCount++;

				byTimeout.insert(entry);

#ifdef DEBUG_RECEIVED
				Log::log(LOG_DEBUG, "Create %s:%d", key, timeout);
#endif
			}
		} else {
			entry= keyIter->second;

			// While the entry is on the byTimeout structure, the expires
			// time must be invariant or the structure becomes logically
			// inconsistent, causing some rather strange failures.
			//
			// So we have to erase the entry from the timeout structure
			// before calling receive() which modifies it.

			byTimeout.erase(entry);
			entry->receive(expires, address);
			byTimeout.insert(entry);

#ifdef DEBUG_RECEIVED
			Log::log(LOG_DEBUG, "Defer %s:%d", key, timeout);
#endif
		}
	}

	scheduleWake.notify_one();
}

void Scheduler::start()
{
	run= true;
	thread= new std::thread(&Scheduler::scheduleLoop, this);

	int numWorkers= 4;

	for (int i= 0; i < numWorkers; i++) {
		WorkerRef w= std::make_shared<Worker>(this);
		workerList.push_back(w);
		w->start();
	}

	Log::log(LOG_DEBUG,
		"Started %d notification worker threads", numWorkers);
}

void Scheduler::stop()
{
	run= false;
	scheduleWake.notify_one();
	thread->join();
	delete thread;

	for (WorkerRef worker : workerList) {
		worker->stop1();
	}

	workerWake.notify_all();

	while (!workerList.empty()) {
		WorkerRef worker= workerList.front();
		workerList.pop_front();
		worker->stop2();
	}
}

EntryRef Scheduler::getWork()
{
	EntryRef rval;
	std::unique_lock<std::mutex> lock(mutex);

	while (run && workerQueue.empty()) {
		workerWake.wait(lock);
	}
	if (run) {
		rval= workerQueue.front();
		workerQueue.pop_front();
	}

	return rval;
}

void Scheduler::scheduleLoop()
{
	bool localRun= run;
	while (localRun) {
		struct timeval now;
		gettimeofday(&now, NULL);

		std::unique_lock<std::mutex> lock(mutex);
		if (!byTimeout.empty()) {
			auto startIter= byTimeout.begin();
			EntryRef entry= *startIter;

#ifdef DEBUG_SCHEDLOOP
			Log::log(LOG_DEBUG, "Head of Line: %s",
				entry->getKey());
#endif

			const struct timeval *nextRun= entry->getExpires();
			if ((nextRun->tv_sec < now.tv_sec) ||
				((nextRun->tv_sec == now.tv_sec) &&
				(nextRun->tv_usec <= now.tv_usec)))
			{
				byTimeout.erase(startIter);

				auto keyIter= byKey.find(entry->getKey());
				assert(keyIter != byKey.end());

#ifdef DEBUG_SCHEDLOOP
				Log::log(LOG_DEBUG, "Removing %s", entry->getKey());
#endif

				byKey.erase(keyIter);
				entryCount--;

				workerQueue.push_back(entry);
				workerWake.notify_one();
			} else {
				entry= nullptr;

				auto d= std::chrono::seconds(nextRun->tv_sec) +
					std::chrono::microseconds(nextRun->tv_usec);
				std::chrono::system_clock::time_point wake{
					std::chrono::duration_cast<
					std::chrono::system_clock::duration>(d)};

#ifdef DEBUG_SCHEDLOOP
				Log::log(LOG_DEBUG, "Scheduler - wait about %d sec",
					nextRun->tv_sec - now.tv_sec);
#endif
				scheduleWake.wait_until(lock, wake);
			}
		} else {
#ifdef DEBUG_SCHEDLOOP
			Log::log(LOG_DEBUG, "Scheduler - indefinite wait");
#endif
			scheduleWake.wait(lock);
		}

		localRun= run;
	}
}

