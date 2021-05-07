class Scheduler;
typedef std::shared_ptr<Scheduler> SchedulerRef;

/**
 * SimpleListener
 *
 * A listener that decodes the simple protocol, which is just a UDP packet
 * containing the key, with an optional colon and timeout at the end.
 */
class SimpleListener : public UdpListener {
public:
	SimpleListener(SchedulerRef, int family, int port, bool isMulticast);
	virtual ~SimpleListener();

	static std::shared_ptr<Listener> Create(
		SchedulerRef scheduler, int family, int port, bool isMulticast)
	{
		return std::make_shared<SimpleListener>(
			scheduler, family, port, isMulticast);
	}

protected:
	virtual void handlePacket(
		char const *data, socklen_t dataLen, char const *address);

private:
	SchedulerRef scheduler;
};

typedef std::shared_ptr<Listener> ListenerRef;

