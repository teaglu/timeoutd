/**
 * UdpSender
 *
 * Parent class for anything that sends over UDP.  This class takes care of
 * the scheduling and socket setup, including using connect() to bind the
 * socket to the destination so it doesn't have to be specified every time.
 */
class UdpSender : public Sender {
private:
	std::thread *thread;

   	std::mutex mutex;
	std::condition_variable wake;
	volatile bool run;

	void sendLoop();

	// How often to send, in seconds.
	int frequency;

	bool isMulticast;

	void bindIp4MulticastInterface(int sock);

	static char const *ignoreInterfacePrefix[];

	// sockaddr_in6 is the largest structure we'd use
	char addrBuffer[sizeof(struct sockaddr_in6)];
	socklen_t addrLen;

	short family;
	void *addrPart;

protected:
	// Implemented by child class to actually build and send the packet.
	virtual void sendPacket(int sock) = 0;

	// Common routine to parse an address
	static bool ParseAddress(
		char const *text,
		int port,
		struct sockaddr *address,
		socklen_t &addressLen);

public:
	UdpSender(
		struct sockaddr *addr,
		socklen_t addrLen,
		int frequency);

	virtual ~UdpSender();

	virtual bool start();
	virtual void stop();
};

