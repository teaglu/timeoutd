class Scheduler;
typedef std::shared_ptr<Scheduler> SchedulerRef;

class SignedListener;

/**
 * PreSharedKey
 *
 * PreSharedKey is just a wrapper for a zero-padded key ready for the HMAC
 * routines to use.  We allow multiple valid keys so we can phase in a new
 * key while keeping the old one valid.
 */
class PreSharedKey {
public:
	PreSharedKey(char const *);

protected:
	char key[32];

	friend class SignedListener;
};
typedef std::shared_ptr<PreSharedKey> PreSharedKeyRef;

/**
 * SignedListener
 *
 * A SignedListener listens for and decodes UDP packets using the HMAC
 * signed protocol defined in protocol.h
 */
class SignedListener : public UdpListener {
public:
	SignedListener(SchedulerRef, int family, int port, bool isMulticast);
	virtual ~SignedListener();

	static std::shared_ptr<SignedListener> Create(
		SchedulerRef scheduler, int family, int port, bool isMulticast)
	{
		return std::make_shared<SignedListener>(
			scheduler, family, port, isMulticast);
	}

	void addPreSharedKey(char const *key) {
		keyList.push_back(std::make_shared<PreSharedKey>(key));
	}

protected:
	// Validate the header and HMAC
	bool validateHeader(
		unsigned char *data, socklen_t dataLen, char const *address);

	// Handle the payload after the header.
	void handlePayload(
		char const *data, socklen_t dataLen, char const *address);

	// Handle the packet from the socket
	virtual void handlePacket(
		char const *data, socklen_t dataLen, char const *address);

private:
	SchedulerRef scheduler;

	std::list<PreSharedKeyRef> keyList;
};

typedef std::shared_ptr<SignedListener> SignedListenerRef;
