#define MCAST_ADDRESS "239.42.173.94"

class MulticastInterface {
public:
	MulticastInterface(char const *, struct in_addr &);
	virtual ~MulticastInterface();

	char const *getName() {
		return name.c_str();
	}
	struct in_addr const *getAddress() {
		return &address;
	}
private:
	std::string name;
	struct in_addr address;
};
typedef std::shared_ptr<MulticastInterface> MulticastInterfaceRef;

class Multicast {
private:
	static struct in_addr ip4Group;

	static MulticastInterfaceRef sendInterface;
	static std::list<MulticastInterfaceRef> receiveInterfaces;
	static socklen_t ip4Ttl;

	static char const *ignoreInterfacePrefix[];

	static bool scanInterfaces(int sock, struct ifconf &conf);


public:
	static bool init(int ip4Ttl);

	static void setupSender(int sock);
	static void setupReceiver(int sock);	
};
