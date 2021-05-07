#include <stdlib.h>
#include <stdio.h>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <list>
#include <map>
#include <set>

#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/uio.h>
#include <math.h>
#include <string.h>

#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/sockios.h>

#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>

#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG

#include <assert.h>
