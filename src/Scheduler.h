class Entry;
typedef std::shared_ptr<Entry> EntryRef;

class Worker;
typedef std::shared_ptr<Worker> WorkerRef;

// "less" comparator based on string comparison of char const *
struct CompareConstChar {
	bool operator()(char const * a, char const * b) {
		return strcmp(a, b) < 0;
	}
};

/**
 * Scheduler
 *
 * The scheduler keeps track of the database of entries, and continuously
 * loops on the one with the earliest timeout.  If an entry expires, the
 * scheduler removes the key and schedules it on a work queue for notification.
 */
class Scheduler {
public:
	Scheduler(int entryLimit);
	virtual ~Scheduler();

	void receive(char const *key, int timeout, char const *address);

	void start();
	void stop();

	// Called by worker threads to get new work items
	EntryRef getWork();


	static std::shared_ptr<Scheduler> Create(int entryLimit) {
		return std::make_shared<Scheduler>(entryLimit);
	}

private:
	// We're using a map for key->entry, but to avoid burning memory to
	// store a superfluous copy of an invariant key, we just use a raw
	// "char const *" as the key type.
	//
	// This means when inserting the key we have to make sure to reference the
	// memory from the entry itself via getKey().

	std::map<char const *, std::shared_ptr<Entry>, CompareConstChar> byKey;

	// Normally this would be a priority queue, but std::priority_queue
	// doesn't have a way to remove from mid-heap, which since our main
	// state is lots of re-scheduling won't work.
	//
	// It seems weird to use a std::set here, but the O() operations do
	// what we need.  EntryCompareTimeout is written to use timeout then
	// key to make sure the set is unique.

	std::set<std::shared_ptr<Entry>, EntryCompareTimeout> byTimeout;

	int entryCount;
	int entryLimit;

	// Lock for everything
	std::mutex mutex;

	// Indicator to keep running
	volatile bool run;

	// There's a set of workers that do the actual notifications, and the
	// scheduling thread keeps processing without blocking.

	// List of entries waiting on a worker
	std::list<EntryRef> workerQueue;

	// Condition signalling available work
	std::condition_variable workerWake;

	// List of workers
	std::list<WorkerRef> workerList;

	// Main schedule loop
	void scheduleLoop();

	// Scheduling thread
	std::thread *thread;

	// Condition to kick off a scheduling thread re-eval
	std::condition_variable scheduleWake;
};

typedef std::shared_ptr<Scheduler> SchedulerRef;
