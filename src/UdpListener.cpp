#include "system.h"

#include "Log.h"
#include "Entry.h"
#include "Scheduler.h"
#include "Listener.h"
#include "UdpListener.h"

#include "Multicast.h"

#define RECV_BUFFER_SIZE 2047

UdpListener::UdpListener(
	int family,
	int port,
	bool isMulticast)
{
	this->family= family;
	this->port= port;
	this->isMulticast= isMulticast;

	assert((family == AF_INET) || (family == AF_INET6));

	familyName= (family == AF_INET) ? "IP4" : "IP6";
}

UdpListener::~UdpListener()
{
}

void UdpListener::listenLoop()
{
	int sock= socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == -1) {
		Log::log(LOG_ERROR,
			"Unable to create an %s UDP socket: %s",
			familyName, strerror(errno));
	} else {
		struct sockaddr_in6 addrBuffer;
		socklen_t addrLen;
		void *addrPart;

		if (family == AF_INET) {
			addrLen= sizeof(struct sockaddr_in);
			struct sockaddr_in *addr=
				reinterpret_cast<struct sockaddr_in *>(&addrBuffer);

			addrPart= &addr->sin_addr;

			addr->sin_family= AF_INET;
			addr->sin_addr.s_addr= INADDR_ANY;
			addr->sin_port= htons(port);

			if (isMulticast) {
				Multicast::setupReceiver(sock);
			}
		} else {
			addrLen= sizeof(struct sockaddr_in6);
			struct sockaddr_in6 *addr=
				reinterpret_cast<struct sockaddr_in6 *>(&addrBuffer);

			addrPart= &addr->sin6_addr;

			addr->sin6_family= AF_INET6;
			addr->sin6_addr= in6addr_any;
			addr->sin6_port= htons(port);

			addr->sin6_flowinfo= 0; // Wat?
			addr->sin6_scope_id= 0;

			int bind6Only= 1;
			if (setsockopt(sock,
				IPPROTO_IPV6, IPV6_V6ONLY,
				&bind6Only, sizeof(int)) == -1)
			{
				Log::log(LOG_ERROR,
					"Unable to set IPV6_V6ONLY - bind will probably fail: %s",
					strerror(errno));
			}
		}

		struct sockaddr *addr=
			reinterpret_cast<struct sockaddr *>(&addrBuffer);

		if (bind(sock, addr, addrLen) == -1) {
			Log::log(LOG_ERROR,
				"Unable to bind %s UDP port %d: %s",
				familyName, port, strerror(errno));
		} else {
			Log::log(LOG_DEBUG,
				"Listening on %s UDP port %d", familyName, port);

			bool localRun= run;
			while (localRun) {
				fd_set readFds;
				FD_ZERO(&readFds);
				FD_SET(sock, &readFds);
				FD_SET(stopPipe[0], &readFds);

				int selectRval= select(
					FD_SETSIZE, &readFds, NULL, NULL, NULL);

				if (selectRval == -1) {
					Log::log(LOG_ERROR,
						"Error in select waiting for data: %s",
						strerror(errno));
					sleep(10);
				} else if (selectRval > 0) {
					if (FD_ISSET(sock, &readFds)) {
						char data[RECV_BUFFER_SIZE];

						addrLen= sizeof(addrBuffer);
						int dataLen= recvfrom(sock, data,
							RECV_BUFFER_SIZE, MSG_DONTWAIT,
							addr, &addrLen);

						if (dataLen < 0) {
							Log::log(LOG_WARNING,
								"Error reading data packet: %s",
								strerror(errno));
						} else if (dataLen > 0) {
							char addrString[INET6_ADDRSTRLEN];
							inet_ntop(family,
								addrPart, addrString, INET_ADDRSTRLEN);

							handlePacket(data, dataLen, addrString);
						}
					}
				}

				std::lock_guard<std::mutex> guard(runLock);
				localRun= run;
			}

			close(sock);
		}
	}
}

bool UdpListener::start()
{
	if (pipe(stopPipe) == -1) {
		Log::log(LOG_ERROR,
			"Unable to create UDP stop pipe pair: %s",
			strerror(errno));
	}


	run= true;
	thread= new std::thread(&UdpListener::listenLoop, this);
	return true;
}

void UdpListener::stop()
{
	run= false;
	if (write(stopPipe[1], "\0", 1) == -1) {
		Log::log(LOG_ERROR,
			"Error writing to UDP stop pipe: %s",
			strerror(errno));
	}

	thread->join();
	delete thread;
	thread= NULL;

	close(stopPipe[0]);
	close(stopPipe[1]);
}


