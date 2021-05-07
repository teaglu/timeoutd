/**
 * SignedSender
 *
 * A sender that sends packets encoded using the HMAC-authenticated protocol
 * defined in protocol.h
 */
class SignedSender : public UdpSender {
protected:
	virtual void sendPacket(int sock);

public:
	SignedSender(
		struct sockaddr *addr,
		socklen_t addrLen,
		int frequency,
		char const *preSharedKey,
		char const *key,
		int timeout);

	virtual ~SignedSender();

	static std::shared_ptr<Sender> Create(
		char const *address, int port, int frequency,
		char const *preSharedKey,
		char const *key, int timeout);

private:
	std::string key;
	int timeout;

	unsigned char hmacKey[32];
};

