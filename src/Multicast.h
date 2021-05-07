#define MCAST_ADDRESS "239.42.173.94"

class Multicast {
private:
	static struct in_addr ip4Group;

	static std::string ip4Interface;
	static struct in_addr ip4Address;
	static socklen_t ip4Ttl;

	static char const *ignoreInterfacePrefix[];

	static bool scanInterfaces(int sock, struct ifconf &conf);


public:
	static bool init(int ip4Ttl);

	static void setupSender(int sock);
	static void setupReceiver(int sock);	
};
