#define MCAST_ADDRESS "239.42.173.94"

/**
 * UdpListener
 *
 * Base class for UDP listeners.  This class handles the details of building
 * and listening on the socket, as well as the listen thread.
 */
class UdpListener : public Listener {
private:
	std::thread *thread;

	// On Unix the easiest way to create an interruptible blocking read is
	// to create a pair of pipes, and to interrupt the read write a junk
	// byte onto the write side.  We need this so we can cleanly shut down
	// the reader.
	int stopPipe[2];

	volatile bool run;
	std::mutex runLock;

	int port;
	int family;
	bool isMulticast;

	char const *familyName;

	void listenLoop();

protected:
	// This is the protocol-level packet handler defined in the child class
	virtual void handlePacket(
		char const *data, socklen_t dataLen, char const *address) = 0;

public:
	UdpListener(int family, int port, bool isMulticast);
	virtual ~UdpListener();

	virtual bool start();
	virtual void stop();
};


