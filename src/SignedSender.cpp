#include "system.h"

#include "Log.h"
#include "Sender.h"
#include "UdpSender.h"
#include "SignedSender.h"

#include "protocol.h"

SignedSender::SignedSender(
	struct sockaddr *addr,
	socklen_t addrLen,
	int frequency,
	char const *presharedKey,
	char const *key,
	int timeout) :
	UdpSender(addr, addrLen, frequency)
{
	memset(hmacKey, 0, KEEPALIVE_HMAC_SIZE);
	int presharedKeyLen= strlen(presharedKey);
	memcpy(hmacKey, presharedKey,
		(presharedKeyLen > KEEPALIVE_HMAC_SIZE) ?
		KEEPALIVE_HMAC_SIZE : presharedKeyLen);

	this->key= key;
	this->timeout= timeout;
}

SignedSender::~SignedSender()
{
}

SenderRef SignedSender::Create(
	char const *host,
	int port,
	int frequency,
	char const *preSharedKey,
	char const *key,
	int timeout)
{
	SenderRef rval;

	unsigned char addressBuffer[sizeof(struct sockaddr_in6)];
	struct sockaddr *address=
		reinterpret_cast<struct sockaddr *>(addressBuffer);

	socklen_t addressLen= sizeof(addressBuffer);

	if (ParseAddress(host, port, address, addressLen)) {
		rval= std::make_shared<SignedSender>(
			address, addressLen, frequency, preSharedKey,
			key, timeout);
	}

	return rval;
}

void SignedSender::sendPacket(int sock)
{
	std::string payload= key;
	payload.append(":");
	payload.append(std::to_string(timeout));

	socklen_t bufferLen= sizeof(struct keepalive_hdr) + payload.length();
	unsigned char *buffer= new unsigned char[bufferLen];
	struct keepalive_hdr *header=	
		reinterpret_cast<struct keepalive_hdr *>(buffer);

	time_t now;
	time(&now);

	header->magic= htons(KEEPALIVE_HEADER_MAGIC);
	header->version= htons(KEEPALIVE_HEADER_VERSION);
	header->timestamp= htonl(now);

	memcpy(header + 1, payload.c_str(), payload.length());

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	HMAC_CTX *hmacCtx= HMAC_CTX_new();
#else
	HMAC_CTX localCtx;
	HMAC_CTX_init(&localCtx);
	HMAC_CTX *hmacCtx= &localCtx;
#endif

	// Use 36 bytes so we include the timestamp
	if (!HMAC_Init(hmacCtx, hmacKey, KEEPALIVE_HMAC_SIZE, EVP_sha256())) {
		Log::log(LOG_ERROR, "Failed to init HMAC");
	} else if (!HMAC_Update(hmacCtx, buffer + 36, bufferLen - 36)) {
		Log::log(LOG_ERROR, "Failed to add data to HMAC");
	} else {
		unsigned int hmacLen= KEEPALIVE_HMAC_SIZE;
		if (!HMAC_Final(hmacCtx, header->hmac, &hmacLen)) {
			Log::log(LOG_ERROR, "Failed to finalize HMAC");
		} else {
			int sentLen= send(sock, buffer, bufferLen, 0);

			if (sentLen == -1) {
				// If the destination isn't listening on the port we get a
				// ECONNREFUSED because of the ICMP port unreachable message.
				// This is a normal thing when the other end isn't up yet.
				if (errno != ECONNREFUSED) {
					Log::log(LOG_ERROR,
						"Error in sending signed packet: %s",
						strerror(errno));
				}
			}
		}
	}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
	HMAC_CTX_free(hmacCtx);
#else
	HMAC_CTX_cleanup(&localCtx);
#endif

	delete[] buffer;
}

