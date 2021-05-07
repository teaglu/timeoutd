#include "system.h"

#include "Log.h"
#include "Sender.h"
#include "UdpSender.h"

#include "Multicast.h"

UdpSender::UdpSender(
	struct sockaddr *addr,
	socklen_t addrLen,
	int frequency)
{
	assert(addrLen <= sizeof(addrBuffer));

	memcpy(addrBuffer, addr, addrLen);
	this->addrLen= addrLen;

	this->frequency= frequency;

	// Family is the first 16 in both inet4 and inet6
	struct sockaddr_in *addr4=
		reinterpret_cast<struct sockaddr_in *>(addrBuffer);
	struct sockaddr_in6 *addr6=
		reinterpret_cast<struct sockaddr_in6 *>(addrBuffer);

	family= addr4->sin_family;

	isMulticast= false;
	if (family == AF_INET) {
		isMulticast= IN_MULTICAST(ntohl(addr4->sin_addr.s_addr));
		addrPart= &addr4->sin_addr;
	} else {
		addrPart= &addr6->sin6_addr;
	}
}

bool UdpSender::ParseAddress(
	char const *text,
	int port,
	struct sockaddr *address,
	socklen_t &addressLen)
{
	bool valid= false;

	struct sockaddr_in addr4;
	addr4.sin_family= AF_INET;
	addr4.sin_port= htons(port);

	struct sockaddr_in6 addr6;
	addr6.sin6_family= AF_INET6;
	addr6.sin6_port= htons(port);
	addr6.sin6_flowinfo= 0;
	addr6.sin6_scope_id= 0;

	if (addressLen < sizeof(addr6)) {
		Log::log(LOG_ERROR, "Sockaddr buffer too small for output");
	} else if (inet_pton(AF_INET, text, &addr4.sin_addr)) {
		addressLen= sizeof(addr4);
		memcpy(address, &addr4, sizeof(addr4));
		valid= true;
	} else if (inet_pton(AF_INET6, text, &addr6.sin6_addr)) {
		addressLen= sizeof(addr6);
		memcpy(address, &addr6, sizeof(addr6));
		valid= true;
	} else {
		struct addrinfo *hostInfo;
		struct addrinfo hints;

		hints.ai_flags= AI_ADDRCONFIG;
		hints.ai_family= AF_UNSPEC;
		hints.ai_socktype= SOCK_DGRAM;
		hints.ai_protocol= IPPROTO_UDP;

		if (getaddrinfo(text, NULL, &hints, &hostInfo) == 0) {
			if (hostInfo->ai_addrlen > 0) {
				struct sockaddr_in *found4=
					reinterpret_cast<struct sockaddr_in *>
					(&hostInfo->ai_addr[0]);

				struct sockaddr_in6 *found6=
					reinterpret_cast<struct sockaddr_in6 *>
					(&hostInfo->ai_addr[0]);

				if (found4->sin_family == AF_INET) {
					addr4.sin_addr= found4->sin_addr;

					addressLen= sizeof(addr4);
					memcpy(address, &addr4, sizeof(addr4));
					valid= true;
				} else if (found6->sin6_family == AF_INET6) {
					addr6.sin6_addr= found6->sin6_addr;

					addressLen= sizeof(addr6);
					memcpy(address, &addr6, sizeof(addr6));
					valid= true;
				} else {
					Log::log(LOG_ERROR,
						"Unknown address family %d returned by DNS",
						found4->sin_family);
				}
			} else {
				Log::log(LOG_ERROR,
					"Unable to find DNS record for peer %s",
					text);
			}
			freeaddrinfo(hostInfo);
		} else {
			Log::log(LOG_ERROR,
				"Unable to find DNS record for peer %s", text);
		}
	}

	return valid;
}

void UdpSender::sendLoop()
{
	int sock= socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		Log::log(LOG_ERROR,
			"Unable to create an outgoing UDP socket: %s",
			strerror(errno));
	} else {
		if (isMulticast) {
			Multicast::setupSender(sock);
		}

		// Go ahead and call connect so the sender itself doesn't
		// need to specify the destination

		struct sockaddr const *genericAddr=
			reinterpret_cast<struct sockaddr const *>(&addrBuffer);

		if (connect(sock, genericAddr, addrLen) == -1) {
			Log::log(LOG_ERROR,
				"Unable to connect UDP socket to destination: %s",
				strerror(errno));
		} else {
			{
				char addrString[INET6_ADDRSTRLEN];
				inet_ntop(family,
					addrPart, addrString, INET_ADDRSTRLEN);

				Log::log(LOG_DEBUG,
					"Starting sender to %s, %d seconds",
					addrString, frequency);
			}
				
			bool localRun= run;
			while (localRun) {
				sendPacket(sock);

				std::unique_lock<std::mutex> lock(mutex);
				if ((localRun= run)) {
					wake.wait_for(lock, std::chrono::seconds(frequency));
					localRun= run;
				}
			}
		}

		close(sock);
	}
}

bool UdpSender::start()
{
	run= true;
	thread= new std::thread(&UdpSender::sendLoop, this);
	return true;
}

void UdpSender::stop()
{
	run= false;
	wake.notify_one();

	thread->join();
	delete thread;
	thread= NULL;
}

UdpSender::~UdpSender()
{
}

