/*
 * These routines contain the calls to start up and shut down openssl
 * in such a way that valgrind produces useful output without pages and
 * pages of noise from openssl.
 *
 */

void opensslStartupIncantations();
void opensslShutdownIncantations();
