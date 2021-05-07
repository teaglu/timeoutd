/**
 * SimpleSender
 *
 * A sender that sends to peers using the simple text protocol.
 */
class SimpleSender : public UdpSender {
protected:
	virtual void sendPacket(int sock);

public:
	SimpleSender(
		struct sockaddr *addr,
		socklen_t addrLen,
		int frequency,
		char const *key,
		int timeout);

	virtual ~SimpleSender();

	static std::shared_ptr<Sender> Create(
		char const *address, int port, int frequency,
		char const *key, int timeout);

private:
	std::string key;
	int timeout;
};

