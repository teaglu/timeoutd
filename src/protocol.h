/*
 * This file defines the wire format for the secured version of the UDP
 * protocol.
 */

#define KEEPALIVE_SIMPLE_PORT (2952)
#define KEEPALIVE_SIGNED_PORT (2953)

// Magic number
#define KEEPALIVE_HEADER_MAGIC 0xb049

// Version number
#define KEEPALIVE_HEADER_VERSION 0x0001

// Size of HMAC - this corresponds to SHA-256
#define KEEPALIVE_HMAC_SIZE 32

// Don't let the compiler insert shims (that is, align to 1 byte)
#pragma pack(push, 1)

struct keepalive_hdr {
	// Magic number in network byte order
	unsigned short magic;

	// Version number in network byte order
	unsigned short version;

	// Raw HMAC value.  The HMAC covers from the timestamp onward.
	unsigned char hmac[KEEPALIVE_HMAC_SIZE];

	// Current timestamp as unix time in network byte order
	unsigned long timestamp;
};

#pragma pack(pop)

