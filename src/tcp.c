/* tcp.c: NT/Unix TCP layer for portable socket applications */


/*********************************************************************
This file makes several simplifying assumptions.
All sockets are stream TCP sockets.
All machines are internet hosts.
All addresses are internet IP addresses, type AF_INET, 4 bytes,
or (equivalently) ASCII digits separated by dots.
The host name of interest is always the primary name (no aliases).
The IP address of interest is always the first in the list
of possible IP addresses.
All sockets use the setup protocol: connect() -> listen() + accept().
This file also includes routines for secure sockets, using the openssl library.
As a special exception, I hereby grant permission to link
the code of this program with the OpenSSL library
(or with modified versions of OpenSSL that use the same license as OpenSSL),
and distribute linked combinations including the two.
You must obey the GNU General Public License in all respects
for all of the code used other than OpenSSL.
If you modify this file, you may extend this exception to your version of the
file, but you are not obligated to do so.
If you do not wish to do so, delete this exception statement from your version.
*********************************************************************/


#ifdef MSDOS
@@ This file cannot be compiled under DOS.
#endif
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#define MAXHOSTNAMELEN  256
#define setErrnoNT() (errno = WSAGetLastError())
#else
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>	/* for error-retrieval */
#include <openssl/rand.h>
#include <sys/param.h>
/* for some reason this prototype is missing */
extern char *inet_ntoa(struct in_addr);
extern int inet_aton(const char *, struct in_addr *);
extern int h_errno;
#define setErrnoNT()
#endif /* Unix or NT */

#include "tcp.h"

#ifndef _WIN32
extern int errno;
#endif
static int errno1;


/* Global variables.  See tcp.h for comments. */
char tcp_thisMachineName[MAXHOSTNAMELEN];
char tcp_thisMachineDots[20];
IP32bit tcp_thisMachineIP;
char tcp_farMachineDots[20];
IP32bit tcp_farMachineIP;
short tcp_farMachinePort;


/*********************************************************************
Determine the name and IP address of the current machine.
This is called at init time.
As such, it should include any other code needed to initialize
the TCP stack, or your access to it.
This is the case under NT.
*********************************************************************/

int
tcp_init(void)
{
    int rc;

#ifdef _WIN32
    WSADATA WSAData;
    rc = WSAStartup(0x0101, &WSAData);
    if(rc)
	return -1;
#endif

    rc = gethostname(tcp_thisMachineName, MAXHOSTNAMELEN);
    if(rc)
	return -1;

/* convert from name to IP information */
    tcp_thisMachineIP = tcp_name_ip(tcp_thisMachineName);
    if(tcp_thisMachineIP == -1)
	return -1;
    strcpy(tcp_thisMachineDots, tcp_ip_dots(tcp_thisMachineIP));

    return 0;			/* ok */
}				/* tcp_init */


/*********************************************************************
Routines to convert between names and IP addresses.
*********************************************************************/

IP32bit
tcp_name_ip(const char *name)
{
    struct hostent *hp;
    IP32bit *ip;

    hp = gethostbyname(name);
#if 0
    printf("%s\n", hstrerror(h_errno));
#endif
    if(!hp)
	return NULL_IP;
#if 0
    puts("found it");
    if(hp->h_aliases) {
	puts("aliases");
	char **a = hp->h_aliases;
	while(*a) {
	    printf("alias %s\n", *a);
	    ++a;
	}
    }
#endif
    ip = (IP32bit *) * (hp->h_addr_list);
    if(!ip)
	return NULL_IP;
    return *ip;
}				/* tcp_name_ip */

char *
tcp_name_dots(const char *name)
{
    IP32bit ip = tcp_name_ip(name);
    if(ip == NULL_IP)
	return 0;
    return tcp_ip_dots(ip);
}				/* tcp_name_dots */

char *
tcp_ip_dots(IP32bit ip)
{
    return inet_ntoa(*(struct in_addr *)&ip);
}				/* tcp_ip_dots */

int
tcp_isDots(const char *s)
{
    const char *t;
    char c;
    int nd = 0;			/* number of dots */
    if(!s)
	return 0;
    for(t = s; (c = *t); ++t) {
	if(c == '.') {
	    ++nd;
	    if(t == s || !t[1])
		return 0;
	    if(t[-1] == '.' || t[1] == '.')
		return 0;
	    continue;
	}
	if(!isdigit(c))
	    return 0;
    }
    return (nd == 3);
}				/* tcp_isDots */

IP32bit
tcp_dots_ip(const char *s)
{
    struct in_addr a;
/* Why can't SCO Unix be like everybody else? */
#ifdef SCO
    inet_aton(s, &a);
#else
    *(IP32bit *) & a = inet_addr(s);
#endif
    return *(IP32bit *) & a;
}				/* tcp_dots_ip */

char *
tcp_ip_name(IP32bit ip)
{
    struct hostent *hp = gethostbyaddr((char *)&ip, 4, AF_INET);
    if(!hp)
	return 0;
    return hp->h_name;
}				/* tcp_ip_name */

char *
tcp_dots_name(const char *s)
{
    return tcp_ip_name(tcp_dots_ip(s));
}				/* tcp_dots_name */


/*********************************************************************
Connect to a far machine, or listen for an incoming call.
*********************************************************************/

/* create a new socket */
static int
makeSocket(void)
{
    int s;			/* file descriptor for socket */
    int success = 0;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s != -1) {
	/* set some options, then return the socket number */
	struct linger ling;
	static int yes = 1;
	static int no = 0;
	/* we shouldn't be closing a socket unless we've finished the session.
	 * That is, I asked my last question and I got my last response.
	 * If we are closing it for any other reason, if must be a failure leg.
	 * May as well discard any pending data and close it quickly. */
	ling.l_onoff = 0;
	ling.l_linger = 0;
#ifndef _WIN32
/* What's the Windows equivalent of FD_CLOEXEC, and how do we set it? */
	success = fcntl(s, F_SETFD, FD_CLOEXEC);
	if(success == -1) {
	    setErrnoNT();
	    return -1;
	}
#endif
	setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&ling, sizeof (ling));
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof (yes));
	/* I feel better setting REUSEPORT, but this option isn't available
	 * on every system.  I hope it is the default. */
#ifdef SO_REUSEPORT
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *)&yes, sizeof (yes));
#endif
	/* for now I don't see any advantage in keeping sockets warm. */
	setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *)&no, sizeof (no));

	return s;		/* ok */
    }

    setErrnoNT();
    return -1;
}				/* makeSocket */

int
tcp_connect(IP32bit ip, int portnum, int timeout)
{
    struct sockaddr_in sa;
    int s;			/* file descriptor of socket */

    /* create the socket and make the connection */
    s = makeSocket();
    if(s < 0)
	return s;

    if(timeout) {
	struct timeval s_restrict;
	s_restrict.tv_sec = timeout;
	s_restrict.tv_usec = 0;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &s_restrict,
	   sizeof (s_restrict));
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &s_restrict,
	   sizeof (s_restrict));
    }

    memset(&sa, 0, sizeof (sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = ip;
    sa.sin_port = htons((short)portnum);
    if(connect(s, (struct sockaddr *)&sa, sizeof (sa)) == -1) {
	setErrnoNT();
	errno1 = errno;
	shutdown(s, 2);
	errno = errno1;
	return -1;
    }
    /* connect returned error code */
    return s;
}				/* tcp_connect */

static int listen_s = -1;	/* the listening socket */

int
tcp_listen(int portnum, int once)
{
    int new_s;			/* accepting socket */
    struct sockaddr_in sa;
#ifdef _WIN32
    size_t sasize;
#else
    unsigned int sasize;
#endif
    int tries = 0;

  retry:
    ++tries;

    if(listen_s < 0) {
	listen_s = makeSocket();
	if(listen_s < 0)
	    return -1;

	memset(&sa, 0, sizeof (struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = htons((short)portnum);
	if(bind(listen_s, (struct sockaddr *)&sa, sizeof (sa)) == -1) {
	    setErrnoNT();
	    errno1 = errno;
	    tcp_unlisten();
	    errno = errno1;
	    return -1;
	}
	/* bind error */
	if(listen(listen_s, (once ? 1 : 5)) == -1) {
	    setErrnoNT();
	    errno1 = errno;
	    tcp_unlisten();
	    errno = errno1;
	    return -1;
	}			/* listen error */
    }

    /* opening the listening socket */
    /* wait for the incoming connection */
    sa.sin_family = AF_INET;
    sasize = sizeof (sa);
    errno1 = 0;
    if((new_s = accept(listen_s, (struct sockaddr *)&sa, &sasize)) < 0) {
	setErrnoNT();
	errno1 = errno;
	tcp_unlisten();
	errno = errno1;
	if(once || tries > 1)
	    return -1;
	goto retry;
    }
    /* failure to accept */
    tcp_farMachineIP = sa.sin_addr.s_addr;
    strcpy(tcp_farMachineDots, tcp_ip_dots(tcp_farMachineIP));
    tcp_farMachinePort = ntohs(sa.sin_port);

    if(once)
	tcp_unlisten();
    return new_s;
}				/* tcp_listen */

void
tcp_unlisten(void)
{
    if(listen_s >= 0) {
	shutdown(listen_s, 2);
	listen_s = -1;
    }
}				/* tcp_unlisten */

void
tcp_close(int fh)
{
#ifdef _WIN32
    closesocket(fh);
#else
    close(fh);
#endif
    shutdown(fh, 2);
}				/* tcp_close */


/*********************************************************************
Read and write data on the socket.
*********************************************************************/

int
tcp_read(int fh, char *buf, int buflen)
{
    int n = recv(fh, buf, buflen, 0);
    if(n < 0)
	setErrnoNT();
    return n;
}				/* tcp_read */

int
tcp_readFully(int fh, char *buf, int buflen)
{
    int n = 0;
    int rc;
    while(buflen) {
	rc = tcp_read(fh, buf, buflen);
	if(rc < 0)
	    return rc;
	if(!rc)
	    break;
	n += rc;
	buflen -= rc;
	buf += rc;
    }
    return n;
}				/* tcp_readFully */

int
tcp_write(int fh, const char *buf, int buflen)
{
    int savelen = buflen;
    int n;
    while(buflen) {
	n = send(fh, buf, buflen, 0);
	if(n < 0) {
	    setErrnoNT();
	    return n;
	}
	buf += n, buflen -= n;
    }
    return savelen;
}				/* tcp_write */


/*********************************************************************
Secure sockets.
Assumes one secure socket at a time.
I know, it's not threadsafe, but it's all edbrowse needs.
*********************************************************************/

static SSL_CTX *sslcx;		/* the overall ssl context */
char *sslCerts;
int verifyCertificates = 1;

/* Called from main() */
/* Return -1 if we can't set up the certificates. */
void
ssl_init(void)
{
/* I don't understand any of this. */
    char f_randfile[ABSPATH];
    if(RAND_egd(RAND_file_name(f_randfile, sizeof (f_randfile))) < 0) {
	/* Not an EGD, so read and write to it */
	if(RAND_load_file(f_randfile, -1))
	    RAND_write_file(f_randfile);
    }
    SSLeay_add_ssl_algorithms();
    sslcx = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_options(sslcx, SSL_OP_ALL);

    /*
       Load certificates from the file whose pathname is
       sslCerts.
       The third argument to this function is the name of a directory in which
       certificates are stored, one per file.
       Both the filename and the directory name may be NULL.
       Right now, we only support one large file containing all the
       certificates.  This could be changed easily.
     */

    SSL_CTX_load_verify_locations(sslcx, sslCerts, NULL);

    SSL_CTX_set_default_verify_paths(sslcx);
    SSL_CTX_set_mode(sslcx, SSL_MODE_AUTO_RETRY);
    ssl_verify_setting();
}				/* ssl_init */

void
ssl_verify_setting(void)
{
    if(verifyCertificates)
	SSL_CTX_set_verify(sslcx, SSL_VERIFY_PEER, NULL);
    else
	SSL_CTX_set_verify(sslcx, SSL_VERIFY_NONE, NULL);
}				/* ssl_verify_setting */

static SSL *secstream;		/* secure stream */

int
ssl_newbind(int fd)
{
    int err;

    secstream = SSL_new(sslcx);
    SSL_set_fd(secstream, fd);
/* Do we need this?
secstream->options |= SSL_OP_NO_TLSv1;
*/
    err = SSL_connect(secstream);
    if(err != 1) {
	err = ERR_peek_last_error();
	ERR_clear_error();
	ssl_done();
	close(fd);
	if(ERR_GET_REASON(err) == SSL_R_CERTIFICATE_VERIFY_FAILED)
	    return -999;
	else
	    return err;
    }

    return 0;
}				/* ssl_newbind */

void
ssl_done(void)
{
    SSL_free(secstream);
}				/* ssl_done */

/* This is similar to our tcp_readFully */
int
ssl_readFully(char *buf, int len)
{
    int pos = 0;
    int n;
    while(len) {
	n = SSL_read(secstream, buf + pos, len);
	if(n < 0)
	    return n;
	if(!n)
	    return pos;
	len -= n, pos += n;
    }
    return pos;			/* filled the whole buffer */
}				/* ssl_readFully */

int
ssl_read(char *buf, int len)
{
    return SSL_read(secstream, buf, len);
}				/* ssl_read */

int
ssl_write(const char *buf, int len)
{
    return SSL_write(secstream, buf, len);
}				/* ssl_write */


/*********************************************************************
Test driver program.
*********************************************************************/

#ifdef TEST_CONNECT

#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
    IP32bit ip;
    int port;
    int fh;
    int nb;			/* number of bytes */
    char buf[256];

    tcp_init();
    ip = tcp_thisMachineIP;
    printf("%s\n%s\n%08lx\n", tcp_thisMachineName, tcp_thisMachineDots, ip);

    if(tcp_name_ip(tcp_ip_name(ip)) != ip)
	puts("fail 1");
    if(tcp_dots_ip(tcp_ip_dots(ip)) != ip)
	puts("fail2");
    if(strcmp(tcp_thisMachineDots,
       tcp_name_dots(tcp_dots_name(tcp_thisMachineDots))))
	puts("fail3");

    if(argc == 1 || argv[1][1] || !strchr("cl", argv[1][0])) {
	fprintf(stderr, "usage:  tcptest l portnum   # listen on a port\n\
        tcptest c machine portnum   # connect to a machine\n");
	return 0;
    }

    if(argv[1][0] == 'l') {
	port = atoi(argv[2]);
	fh = tcp_listen(port, 1);
	if(fh < 0) {
	    fprintf(stderr, "listen[%d] failed, errno %d\n", port, errno);
	    exit(0);
	}

	while(nb = tcp_read(fh, buf, sizeof (buf))) {
	    if(nb < 0) {
		fprintf(stderr, "read failed, errno %d\n", errno);
		exit(0);
	    }
	    buf[nb] = 0;
	    puts(buf);
	}
	exit(0);
    }
    /* listen */
    ip = tcp_name_ip(argv[2]);
    port = atoi(argv[3]);
    if(ip == NULL_IP) {
	fprintf(stderr, "no such machine %s\n", argv[2]);
	exit(0);
    }

    fh = tcp_connect(ip, port, 0);
    if(fh < 0) {
	fprintf(stderr, "connect %s[%d] failed, errno %d\n", argv[2], port,
	   errno);
	exit(0);
    }

    while(fgets(buf, sizeof (buf), stdin)) {
	nb = strlen(buf);
	if(nb && buf[nb - 1] == '\n')
	    --nb;
	nb = tcp_write(fh, buf, nb);
	if(nb < 0) {
	    fprintf(stderr, "write failed, errno %d\n", errno);
	    exit(0);
	}
    }

    tcp_close(fh);
    exit(0);
}				/* main */

#endif
