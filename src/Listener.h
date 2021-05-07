// Multicast address we're using.  This is just picked randomly from the
// private multicast range, and is used by all Teaglu software.

#define MCAST_ADDRESS "239.42.173.94"

/**
 * Listener
 *
 * Generic interface for something that listens, and can be started and
 * stopped.
 */
class Listener {
private:
public:
	Listener();
	virtual ~Listener();

	virtual bool start() = 0;
	virtual void stop() = 0;
};

typedef std::shared_ptr<Listener> ListenerRef;

