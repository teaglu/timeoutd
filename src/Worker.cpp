#include "system.h"

#include "Log.h"

#include "Entry.h"
#include "Scheduler.h"
#include "Worker.h"

Worker::Worker(Scheduler *scheduler)
{
	this->scheduler= scheduler;
}

Worker::~Worker()
{
}

void Worker::start()
{
	run= true;
	thread= new std::thread(&Worker::loop, this);
}

void Worker::stop1()
{
	std::lock_guard<std::mutex> lock(mutex);
	run= false;
}

void Worker::stop2()
{
	thread->join();
	delete thread;
}

void Worker::loop()
{
	bool localRun= run;

	while (localRun) {
		EntryRef entry= scheduler->getWork();

		if (entry) {
			entry->notify();
		}

		std::lock_guard<std::mutex> lock(mutex);
		localRun= run;
	}
}

