class Scheduler;

/**
 * Worker
 *
 * A notification worker.  The scheduler maintains a fixed set of these for
 * doing the actual notifications.
 */
class Worker {
public:
	Worker(Scheduler *);
	virtual ~Worker();

	void start();

	// stop1 flags the worker to stop, and stop2 joins and collects the thread
	//
	// This is necessary because the workers block in Scheduler::getWork, so
	// the worker itself has no clean way to kick that loose.  Since the
	// scheduler is controlling the stop anyway, it was easier to let it
	// control everything.

	void stop1();
	void stop2();

private:
	// Raw pointer instead of std::shared_ptr so we don't create a reference
	// loop that prevents auto-disposal.  The workers are owned by the
	// schedulers and destroyed first.
	Scheduler *scheduler;

	std::mutex mutex;
	volatile bool run;

	std::thread *thread;

	// Work loop
	void loop();
};

typedef std::shared_ptr<Worker> WorkerRef;

