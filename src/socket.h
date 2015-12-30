/* socket.h: header file for NT/Unix socket access layer */

#ifndef SOCKET_H
#define SOCKET_H 1

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
/* include this for htons() and its siblings */
#include <netinet/in.h>
#endif

typedef unsigned int IP32bit;
#define NULL_IP (IP32bit)(-1)

/* Maximum absolute path name. */
#define ABSPATH 264

/* Name of the current machine, as in tuvok.intellivoice.com */
extern char inet_thisMachineName[];

/* IP address of the current machine, both packed and displayable. */
extern char inet_thisMachineDots[];	/* "192.128.25.1" */
extern IP32bit inet_thisMachineIP;	/* pack the above string */
extern char inet_farMachineDots[];	/* like the above, but for the far machine */
extern IP32bit inet_farMachineIP;
extern short inet_farMachinePort;

/* Set up the TCP stack and initialize the above variables */
/* Returns 0 (ok) or -1 (with errno set) */
int tcp_init(void);

/* routines to convert between names and IP addresses */
int inet_isDots(const char *s);
IP32bit inet_name_ip(const char *name);
char *inet_ip_name(IP32bit packed_ip);
char *inet_name_dots(const char *name);
char *inet_dots_name(const char *displayable_ip);
char *inet_ip_dots(IP32bit packed_ip);
IP32bit inet_dots_ip(const char *displayable_ip);

/* Connect to a far machine by tcp.  Use one of the above routines to
 * convert to the packed IP address of the far machine.
 * Returns the socket handle, or -1 if there was a problem. */
int tcp_connect(IP32bit far_ip, int far_portnum, int timeout);

/* Listen for an incoming connection.
 * We expect only one such connection at a time.
 * Returns the socket handle, or -1 if there was a problem. */
int tcp_listen(int portnum, int once);
void tcp_unlisten(void);

/* Read and write data on the socket.
 * returns the number of bytes read, or -1 if there was a problem. */
int tcp_read(int handle, char *buf, int buflen);
int tcp_readFully(int handle, char *buf, int buflen);
int tcp_write(int handle, const char *buf, int buflen);

/* Close the socket */
void tcp_close(int handle);

/* Need a comparable layer for udp */

/* Routines to establish, read, and write secure sockets. */
/* We should not need these for internal use, and curl manages https. */
#if 0
extern char *sslCerts;		/* ssl certificates to validate the secure server */
extern int verifyCertificates;	/* is a certificate required for the ssl connection? */
void ssl_init(void);
void ssl_verify_setting(void);
int ssl_newbind(int fd);
void ssl_done(void);
int ssl_read(char *buf, int len);
int ssl_write(const char *buf, int len);
int ssl_readFully(char *buf, int len);
#endif

#endif
