/* tcp.h: header file for NT/Unix TCP access layer */

#ifndef TCP_H
#define TCP_H 1

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
/* include this for htons() and its siblings */
#include <netinet/in.h>
#endif

/* Name of the current machine, as in tuvok.intellivoice.com */
extern char tcp_thisMachineName[];

/* IP address of the current machine, packed and displayable. */
extern char tcp_thisMachineDots[];	/* "192.128.25.1" */
extern long tcp_thisMachineIP;	/* pack the above string into a long */
extern char tcp_farMachineDots[];	/* like the above, but for the far machine */
extern long tcp_farMachineIP;
extern short tcp_farMachinePort;

/* Set up the TCP stack and initialize the above variables */
/* Returns 0 (ok) or -1 (with errno set) */
int tcp_init();

/* routines to convert between names and IP addresses */
int tcp_isDots(const char *s);
long tcp_name_ip(const char *name);
char *tcp_ip_name(long packed_ip);
char *tcp_name_dots(const char *name);
char *tcp_dots_name(const char *displayable_ip);
char *tcp_ip_dots(long packed_ip);
long tcp_dots_ip(const char *displayable_ip);

/* Connect to a far machine.  Use one of the above routines to
 * convert to the packed IP address of the far machine.
 * Returns the socket handle, or -1 if there was a problem. */
int tcp_connect(long far_ip, int far_portnum, int timeout);

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

#endif
