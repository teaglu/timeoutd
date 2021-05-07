#include "system.h"

#include "Log.h"
#include "Sender.h"
#include "UdpSender.h"
#include "SimpleSender.h"

SimpleSender::SimpleSender(
	struct sockaddr *addr,
	socklen_t addrLen,
	int frequency,
	char const *key,
	int timeout) :
	UdpSender(addr, addrLen, frequency)
{
	this->key= key;
	this->timeout= timeout;
}

SenderRef SimpleSender::Create(
	char const *host,
	int port,
	int frequency,
	char const *key,
	int timeout)
{
	SenderRef rval;

	unsigned char addressBuffer[sizeof(struct sockaddr_in6)];
	struct sockaddr *address=
		reinterpret_cast<struct sockaddr *>(addressBuffer);

	socklen_t addressLen= sizeof(addressBuffer);

	if (ParseAddress(host, port, address, addressLen)) {
		rval= std::make_shared<SimpleSender>(
			address, addressLen, frequency,
			key, timeout);
	}

	return rval;
}

void SimpleSender::sendPacket(int sock)
{
	std::string data= key;
	data.append(":");
	data.append(std::to_string(timeout));

	int sentLen= send(sock, data.c_str(), data.length(), 0);

	if (sentLen == -1) {
		// If the destination isn't listening on the port we get a
		// ECONNREFUSED because of the ICMP port unreachable message.
		// This is a normal thing when the other end isn't up yet.
		if (errno != ECONNREFUSED) {
			Log::log(LOG_ERROR,
				"Error sending UDP packet: %s",
				strerror(errno));
		}
	}
}

SimpleSender::~SimpleSender()
{
}

