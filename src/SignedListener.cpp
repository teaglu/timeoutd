#include "system.h"

#include "Log.h"
#include "Entry.h"
#include "Scheduler.h"
#include "Listener.h"
#include "UdpListener.h"
#include "SignedListener.h"

#include "protocol.h"

// This is how far off the timestamp can be and still be accepted.  Some
// of the skew is transit time, but it's not uncommon for hosts to be
// not quite in sync.
//
// While not having your hosts synced is a bad plan, it's not really the
// domain of this program to make a fuss about it.

#define TIMESTAMP_SLACK 30

SignedListener::SignedListener(
	SchedulerRef scheduler,
	int family,
	int port,
	bool isMulticast) :
	UdpListener(family, port, isMulticast)
{
	this->scheduler= scheduler;
}

SignedListener::~SignedListener()
{
}

bool SignedListener::validateHeader(
	unsigned char *data, socklen_t dataLen, char const *address) 
{
	bool valid= false;
	const struct keepalive_hdr * hdr=
		reinterpret_cast<struct keepalive_hdr *>(data);

	time_t now;
	time(&now);

	time_t timestamp= ntohl(hdr->timestamp);
	if (ntohs(hdr->magic) != KEEPALIVE_HEADER_MAGIC) {
		Log::log(LOG_WARNING,
			"Packet from %s has invalid magic",
			address);
	} else if (ntohs(hdr->version) != KEEPALIVE_HEADER_VERSION) {
		Log::log(LOG_WARNING,
			"Packet from %s has invalid version",
			address);
	} else if ((timestamp > now) && ((timestamp - now) > TIMESTAMP_SLACK)) {
		Log::log(LOG_WARNING,
			"Packet from %s has Timestamp too far in the future",
			address);
	} else if ((now > timestamp) && ((now - timestamp) > TIMESTAMP_SLACK)) {
		Log::log(LOG_WARNING,
			"Packet from %s has Timestamp too far in the past",
			address);
	} else {
		// In >= 1.1 we have to use HMAC_CTX_new and HMAC_CTX_free, but those
		// don't exist in some older versions I see on Centos 7.

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		HMAC_CTX *hmacCtx= HMAC_CTX_new();
#else
		HMAC_CTX localCtx;
		HMAC_CTX_init(&localCtx);
		HMAC_CTX *hmacCtx= &localCtx;
#endif

		for (auto keyI= keyList.begin();
				!valid && (keyI != keyList.end()); ++keyI)
		{
			PreSharedKeyRef k= *keyI;

			// Use 36 bytes so we include the timestamp
			if (!HMAC_Init(hmacCtx, k->key, KEY_LENGTH, EVP_sha256())) {
				Log::log(LOG_ERROR, "Failed to init HMAC");
			} else if (!HMAC_Update(hmacCtx, data + 36, dataLen - 36)) {
				Log::log(LOG_ERROR, "Failed to add data to HMAC");
			} else {
				unsigned char testHmac[HMAC_LENGTH];
				unsigned int testHmacLen= HMAC_LENGTH;
				if (!HMAC_Final(hmacCtx, testHmac, &testHmacLen)) {
					Log::log(LOG_ERROR, "Failed to finalize HMAC");
				} else {
					if (memcmp(testHmac, hdr->hmac, HMAC_LENGTH) == 0) {
						valid= true;
					}
				}
			}
		}

		if (!valid) {
			Log::log(LOG_ERROR,
				"Packet from %s did not match any known pre-shared key",
				address);
		}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		HMAC_CTX_free(hmacCtx);
#else
		HMAC_CTX_cleanup(&localCtx);
#endif
	}

	return valid;
}

void SignedListener::handlePacket(
	char const *data,
	socklen_t dataLen,
	char const *address)
{
	socklen_t headerSize= sizeof(struct keepalive_hdr);

	if (dataLen < headerSize) {
		Log::log(LOG_WARNING, "Received packet shorted than header size");
	} else if (validateHeader((unsigned char *)data, dataLen, address)) {
		handlePayload(&data[headerSize], dataLen - headerSize, address);
	}
}


void SignedListener::handlePayload(
	char const *data,
	socklen_t dataLen,
	char const *address)
{
	bool validName= true;
	for (socklen_t i= 0; validName && (i < dataLen); i++) {
		if (!isalnum(data[i]) && (data[i] != '.') && (data[i] != ':')) {
			Log::log(LOG_WARNING,
				"Invalid ASCII in packet at position %d", i);

			validName= false;
		}
	}

	if (validName) {
		std::string key(data, dataLen);
		int timeout= 30;

		size_t offset= key.find(':');
		if (offset != std::string::npos) {
			std::string timeoutString= key.substr(offset + 1);
			key= key.substr(0, offset);
			timeout= atoi(timeoutString.c_str());
		}

		scheduler->receive(key.c_str(), timeout, address);
	}
}

PreSharedKey::PreSharedKey(char const *value)
{
	memset(key, 0, KEY_LENGTH);
	strncpy(key, value, KEY_LENGTH);
}

