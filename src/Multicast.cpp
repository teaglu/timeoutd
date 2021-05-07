#include "system.h"

#include "Multicast.h"
#include "Log.h"

struct in_addr Multicast::ip4Group;
std::string Multicast::ip4Interface;
struct in_addr Multicast::ip4Address= { INADDR_ANY };

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
        &ip4Address, sizeof(ip4Address)) == -1)
    {
        Log::log(LOG_ERROR,
            "Unable to bind multicast to interface %s: %s",
            ip4Interface.c_str(), strerror(errno));
    }
}

void Multicast::setupReceiver(int sock)
{
	struct ip_mreq group;

	group.imr_multiaddr.s_addr= ip4Group.s_addr;
	group.imr_interface.s_addr= ip4Address.s_addr;
	if (setsockopt(sock, IPPROTO_IP,
		IP_ADD_MEMBERSHIP, (char *)&group, sizeof(group)) < 0)
	{
		Log::log(LOG_ERROR,
			"Failed to enable multicast group: %s",
			strerror(errno));
	} else {
		Log::log(LOG_DEBUG,
			"Subscribed to multicast group %s",
			MCAST_ADDRESS);
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
// So far multicast in docker doesn't really work, but we might want to
// run multicast on the hosts, so throw out the docker* interfaces that
// have RFC1918 address but don't really go anywhere.

char const *Multicast::ignoreInterfacePrefix[]= {
	"lo",
	"docker",
	NULL
};

bool Multicast::scanInterfaces(int sock, struct ifconf &conf)
{
	bool success= true;
	int interfaceCnt= conf.ifc_len / sizeof(struct ifreq);

	for (int i= 0; i < interfaceCnt; i++) {
		struct ifreq *e= &conf.ifc_ifcu.ifcu_req[i];

		if (ioctl(sock, SIOCGIFADDR, e) == -1) {
			Log::log(LOG_CRITICAL,
				"Error: SIOCGIFADDR read data failed: %s",
				strerror(errno));

			success= false;
		} else {
			unsigned long addr= ntohl(
				((struct sockaddr_in *)&e->
				ifr_ifru.ifru_addr)->sin_addr.s_addr);
			unsigned long mask= ntohl(
				((struct sockaddr_in *)&e->
				ifr_ifru.ifru_netmask)->sin_addr.s_addr);

			addr= addr & mask;

			size_t ifNameLen= strlen(e->ifr_name);

			bool use= true;
			for (char const **ti= ignoreInterfacePrefix;
				*ti != NULL; ti++)
			{
				size_t testNameLen= strlen(*ti);
				if (ifNameLen >= testNameLen) {
					if (strncmp(e->ifr_name, *ti, testNameLen) == 0) {
						use= false;
					}
				}
			}

			if (use) {
				use= false;
				if ((addr & 0xFF000000) == 0x0A000000) {
					// 10.0.0.0 - 10.255.255.255
					use= true;
				} else if ((addr & 0xFFFF0000) == 0xC0A80000) {
					// 192.168.0.0 - 192.168.255.255
					use= true;
				} else if ((addr & 0xFFF00000) == 0xAC100000) {
					// 172.16.0.0 - 172.31.255.255
					use= true;
				}
			}

			Log::log(LOG_DEBUG,
				"Interface %s %s a multicast candidate",
				e->ifr_name, use ? "IS" : "is not");

			if (use) {
				if (!ip4Interface.empty()) {
					Log::log(LOG_WARNING,
						"Multiple multicast-eligible interfaces");

					success= false;
				} else {
					ip4Interface= e->ifr_name;
					ip4Address.s_addr=
						((struct sockaddr_in *)&e->
						ifr_ifru.ifru_addr)->sin_addr.s_addr;
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

	if (ip4Interface.empty()) {
		Log::log(LOG_WARNING,
			"Unable to find an RFC1918 interface to bind multicast");

		success= false;
	} else {
		Log::log(LOG_DEBUG,
			"Selected interface %s for multicast operations",
			ip4Interface.c_str());
	}

	return success;
}

