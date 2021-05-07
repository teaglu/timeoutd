#include "system.h"
#include "OpensslMagic.h"

/*
 * For openssl to be thread-safe in any way you have to set up these
 * callbacks to give it a locking mechanism.  This just protects global
 * stuff like algorithm lookups by name - you still have to do your
 * own locking around SSL structures.
 */
static int opensslLockCnt;
static pthread_mutex_t *opensslLock= NULL;

// Because these are only referenced by pointer, some compiler versions
// throw a warning under -Wunused-function

static unsigned long opensslIdCallback() __attribute__((unused));
static void opensslLockCallback(
	int, int, char const *, int) __attribute((unused));


static unsigned long opensslIdCallback()
{
	return (unsigned long)pthread_self();
}

static void opensslLockCallback( int mode, int type, char const *, int)
{
	if( mode & CRYPTO_LOCK ){
		pthread_mutex_lock(&opensslLock[type]);
	} else {
		pthread_mutex_unlock(&opensslLock[type]);
	}
}

void opensslStartupIncantations()
{
	SSL_library_init();
	SSL_load_error_strings();

	// This makes it so you can't use SHA1 any more.  We do need to
	// revisit the hashing algorithm, but until then we can't do this.

	/* FIPS_mode_set(1); */

	// Current docs seem to indicate you don't have to do this any more

	/* OpenSSL_add_all_algorithms(); */
	/* OpenSSL_add_all_ciphers(); */
	/* OpenSSL_add_all_digests(); */

	// Set up the locking stuff

	opensslLockCnt= CRYPTO_num_locks();
	opensslLock= new pthread_mutex_t[opensslLockCnt];
	for (int i= 0; i < opensslLockCnt; i++) {
		pthread_mutex_init(&opensslLock[i], NULL);
	}

	CRYPTO_set_id_callback(opensslIdCallback);
	CRYPTO_set_locking_callback(opensslLockCallback);
}

void opensslShutdownIncantations()
{
	// There have to be a dozen articles on the proper way to run down
	// OpenSSL - all of them different.  I haven't found the magic sauce
	// yet but this is pretty close.
	//
	// From what I've read the OpenSSL team thinks this is a pointless
	// endeavor, but I lean heavily on valgrind to find leaks and having
	// to sort through all that SSL crap is a pain in the ass.

	FIPS_mode_set(0);

	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);

	// With openssl >= 1.1 it registers an atexit handler to clean itself
	// up, and will drop core if you've already called the cleanup routines

	for (int i= 0; i < opensslLockCnt; i++) {
		pthread_mutex_destroy(&opensslLock[i]);
	}
	delete[] opensslLock;

#if OPENSSL_VERSION_NUMBER < 0x10100000L

	ERR_remove_state(0);
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());

	ENGINE_cleanup();
	CONF_modules_free();
	CONF_modules_unload(1);

	/// No defined on Alpine - FIXME look into proper shutdown more
	// COMP_zlib_cleanup();

	ERR_free_strings();
	EVP_cleanup();

	CRYPTO_cleanup_all_ex_data();

#endif

	// valgrind still finds 4 blocks that I can't seem to get to go away,
	// but this seems to be either constant or linear to the number of
	// chains we import?  It's a bunch of error stuff that seems to get
	// pulled in from SSL_CTX_use_certificate_chain_file, but that might
	// just be the first thing that pulls it in.
}

