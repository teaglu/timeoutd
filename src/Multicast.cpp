#include "system.h"

#include "Multicast.h"
#include "Log.h"

#define IFF_802_1Q_VLAN 1<<0
#define IFF_EBRIDGE 1<<1
#define IFF_ISATAP 1<<3

struct in_addr Multicast::ip4Group;

MulticastInterfaceRef Multicast::sendInterface;
std::list<MulticastInterfaceRef> Multicast::receiveInterfaces;


// The default multicast TTL is 1 meaning don't pass outside the broadcast
// domain, even if someone has PIM set up.  The default is currently set to
// 5 in main.cpp to cover one of the more common setups:
//
// 1) server VLAN at site A to route switch at site A
// 2) route switch at site A to firewall at site A
// 3) firewall at site A to firewall at site B over VPN
// 4) firewall at site B to route switch at site B
// 5) route switch at site B to server VLAN at site B
//
socklen_t Multicast::ip4Ttl= 1;

void Multicast::setupSender(int sock)
{
	if (setsockopt(sock, IPPROTO_IP,
		IP_MULTICAST_TTL, &ip4Ttl, sizeof(socklen_t)) == -1)
	{
		Log::log(LOG_ERROR,
			"Error setting multicast TTL: %s",
			strerror(errno));
	}

	if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
		sendInterface->getAddress(), sizeof(struct in_addr)) == -1)
	{
		Log::log(LOG_ERROR,
			"Unable to bind multicast to interface %s: %s",
			sendInterface->getName(), strerror(errno));
	}
}

void Multicast::setupReceiver(int sock)
{
	struct ip_mreq group;

	group.imr_multiaddr.s_addr= ip4Group.s_addr;

	for (MulticastInterfaceRef interface : receiveInterfaces) {
		group.imr_interface.s_addr= interface->getAddress()->s_addr;

		if (setsockopt(sock, IPPROTO_IP,
			IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
		{
			Log::log(LOG_ERROR,
				"Failed to enable multicast group: %s",
				strerror(errno));
		} else {
			Log::log(LOG_DEBUG,
				"Subscribed to %s on %s",
				MCAST_ADDRESS, interface->getName());
		}
	}
}

// Binding IP4 multicast is a lot of "do what I mean" that may not do
// what's expected.  This is the reasoning:
//
// Running multicast for this use on the internet doesn't make a lot of
// sense, so for now we're only looking at interfaces that have an RFC1918
// "private IP address".
//
// Someone could conceivably be running public IPs on a server subnet,
// in which case that would be an invalid assumption.
//
// On a normal application server the loopback would just have 127.0.0.1,
// but if people are running a routing engine they may publish /32's
// in OSPF or something.  So ignore lo* to avoid picking that one.
//
// Multicast in docker appears to work now, you just have to run pimd on
// the host if you want it to pass around like expected.  So we do want
// to receive on docker0 and docker_gwbridge.
//
// We don't pick that as the sending interface, because presumably either:
//
// a) we're running timeoutd on the hosts not in containers, so we want
//    to send to our peers on the actual interface
//
// b) we're running in the container, and our one bridge is going to be
//    the other side of a veth running in docker0 or docker_gwbridge.
//
// Since this is meant to be a failsafe you probably want it pretty close
// to the bottom of your stack instead of inside docker.
//

struct interfaceRule {
	char const *prefix;
	bool receive;
	bool send;
};

static struct interfaceRule interfaceRules[]= {
	{ "lo", 							false,		false },

	// Docker stuff is where it gets convoluted.  We want to listen on
	// the bridge itself to get multicast from the containers.
	{ "docker_gwbridge",				true,		false },
	{ "docker0",						true,		false },

	// These are the host sides of docker bridges
	{ "veth",							false,		false },

	// pimd psuedo-interface
	{ "pimreg",							false,		false },

	// fin
	{ NULL }
};

bool Multicast::scanInterfaces(int sock, struct ifconf &conf)
{
	bool success= true;
	int interfaceCnt= conf.ifc_len / sizeof(struct ifreq);

	for (int i= 0; i < interfaceCnt; i++) {
		struct ifreq *e= &conf.ifc_ifcu.ifcu_req[i];

		if (e->ifr_addr.sa_family != AF_INET) {
			Log::log(LOG_DEBUG, "SKIP");
			continue;
		}

		// Things we need
		struct in_addr ifAddress;
		short ifFlags= 0;
		short ifPrivateFlags= 0;

		if (ioctl(sock, SIOCGIFFLAGS, e) == -1) {
			Log::log(LOG_CRITICAL,
				"SIOCGIFFLAGS failed: %s",
				strerror(errno));

			success= false;
		} else {
			ifFlags= e->ifr_flags;
		}

		// FIXME Eventually I'd like to disregard interfaces based on
		// class flags or similar instead of going by a prefix list.
		// For now this just functions to see what's what in debug mode.
		//
		// Also older kernels don't implements SIOCGIFPFLAGS.

		if (ioctl(sock, SIOCGIFPFLAGS, e) == -1) {
			if (errno == EINVAL) {
				static bool alreadyWarned= false;

				if (!alreadyWarned) {
					Log::log(LOG_ERROR,
						"SIOCGIFPFLAGS->EINVAL - your kernel is too old",
						strerror(errno));
					alreadyWarned= true;
				}
			} else {
				Log::log(LOG_ERROR,
					"Error retrieving private interface flags for %s: %s",
					e->ifr_name, strerror(errno));

				success= false;
			}
		} else {
			ifPrivateFlags= e->ifr_flags;
		}

		Log::log(LOG_DEBUG,
			"Multicast Flags: %s "
			"%cMULTICAST "
			"%c802_1Q_VLAN "
			"%cEBRIDGE "
			"%cISATAP",
			e->ifr_name,
			(ifFlags & IFF_MULTICAST) ? '+' : '-',
			(ifPrivateFlags & IFF_802_1Q_VLAN) ? '+' : '-',
			(ifPrivateFlags & IFF_EBRIDGE) ? '+' : '-',
			(ifPrivateFlags & IFF_ISATAP) ? '+' : '-');

		if (ioctl(sock, SIOCGIFADDR, e) == -1) {
			Log::log(LOG_CRITICAL,
				"SIOCGIFADDR failed: %s",
				strerror(errno));

			success= false;
		} else {
			ifAddress=
				((struct sockaddr_in *)&e->ifr_ifru.ifru_addr)->sin_addr;
		}

		if (success) {
			unsigned long addr= ntohl(
				((struct sockaddr_in *)&e->
				ifr_ifru.ifru_addr)->sin_addr.s_addr);

			size_t ifNameLen= strlen(e->ifr_name);

			bool useReceive= true;
			bool useSend= true;
			for (const struct interfaceRule *rule= interfaceRules;
				rule->prefix != NULL; rule++)
			{
				size_t testLen= strlen(rule->prefix);
				if (ifNameLen >= testLen) {
					if (strncmp(e->ifr_name, rule->prefix, testLen) == 0) {
						if (!rule->send) useSend= false;
						if (!rule->receive) useReceive= false;

						// For now matches are terminating
						break;
					}
				}
			}

			// Determine if the interface IP4 address is in an RFC1918
			// private IP range.

			bool is1918= false;
			if ((addr & 0xFF000000) == 0x0A000000) {
				// 10.0.0.0 - 10.255.255.255
				is1918= true;	
			} else if ((addr & 0xFFFF0000) == 0xC0A80000) {
				// 192.168.0.0 - 192.168.255.255
				is1918= true;
			} else if ((addr & 0xFFF00000) == 0xAC100000) {
				// 172.16.0.0 - 172.31.255.255
				is1918= true;
			}

			useSend= useSend && is1918;

			Log::log(LOG_DEBUG,
				"Multicast: %s %s %s",
				e->ifr_name,
				useSend ? "+SEND" : "-send",
				useReceive ? "+RECEIVE" : "-receive");

			struct in_addr interfaceAddr=
				((struct sockaddr_in *)&e->ifr_ifru.ifru_addr)->sin_addr;

			if (useReceive) {
				receiveInterfaces.push_back(
					std::make_shared<MulticastInterface>(
					e->ifr_name, interfaceAddr));

			}

			if (useSend) {
				if (sendInterface) {
					Log::log(LOG_WARNING,
						"Multiple multicast-eligible interfaces");
				} else {
					sendInterface=
						std::make_shared<MulticastInterface>(
						e->ifr_name, interfaceAddr);
				}
			}
		}
	}
	return success;
}

bool Multicast::init(int ip4Ttl)
{
	Multicast::ip4Ttl= ip4Ttl;

	Log::log(LOG_DEBUG, "Initializing multicast support");

	bool success= true;

	if (!inet_pton(AF_INET, MCAST_ADDRESS, &ip4Group)) {
		Log::log(LOG_ERROR, "Unable to parse multicast address");
		success= false;
	}

	int sock= socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		Log::log(LOG_ERROR,
			"Unable to open sock to query interfaces: %s",
			strerror(errno));

		success= false;
	} else {
		struct ifconf conf;
		conf.ifc_len= 0;
		conf.ifc_ifcu.ifcu_buf= NULL;

		if (ioctl(sock, SIOCGIFCONF, &conf) == -1) {
			Log::log(LOG_ERROR,
				"Error: SIOCGIFCONF get size failed: %s",
				strerror(errno));

			success= false;
		} else {
			conf.ifc_ifcu.ifcu_buf= (char *)malloc(conf.ifc_len);

			if (ioctl(sock, SIOCGIFCONF, &conf) == -1) {
				Log::log(LOG_CRITICAL,
					"Error: SIOCGIFCONF read data failed: %s",
					strerror(errno));

				success= false;
			} else {
				success= success && scanInterfaces(sock, conf);
			}
			free(conf.ifc_ifcu.ifcu_buf);
		}
		close(sock);
	}

	if (!sendInterface) {
		Log::log(LOG_WARNING,
			"Unable to find an RFC1918 interface to bind multicast");

		success= false;
	}

	return success;
}

MulticastInterface::MulticastInterface(
	char const *name,
	struct in_addr &address)
{
	this->name= name;
	this->address= address;
}


MulticastInterface::~MulticastInterface()
{
}


