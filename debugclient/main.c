/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors:  Derick Rethans <d.rethans@jdimedia.nl>                     |
   +----------------------------------------------------------------------+
 */

#include <stdio.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#else
#include <winsock2.h>
#endif
#include <stdlib.h>
#include <sys/stat.h>

#include "../usefulstuff.h"

#ifdef WIN32
#define MSG_NOSIGNAL 0
#define sleep(t)  Sleep((t)*1000)
#define close(fd) closesocket(fd)
#endif

#define VERSION "0.7.0"

int main(int argc, char *argv[])
{
	int port = 17869;
	int ssocket = 0;
	struct sockaddr_in  server_in;
	int                 fd;
	struct sockaddr_in  client_in;
	int                 client_in_len;
	struct in_addr     *iaddr;
	char *buffer;
	char *cmd;
	fd_buf cxt = { NULL, 0 };
	fd_buf std_in = { NULL, 0 };
#ifdef WIN32
	WORD               wVersionRequested;
	WSADATA            wsaData;

	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
#endif

	printf ("Xdebug GDB emulation client (%s)\n", VERSION);
	printf ("Copyright 2002 by Derick Rethans, JDI Media Solutions.\n");

	while (1) {
		ssocket = socket (AF_INET, SOCK_STREAM, 0);
		if (ssocket < 0) {
			printf ("socket: couldn't create socket\n");
			exit(-1);
		}
	
		memset (&server_in, 0, sizeof(struct sockaddr));
		server_in.sin_family      = AF_INET;
		server_in.sin_addr.s_addr = htonl(INADDR_ANY);
		server_in.sin_port        = htons((unsigned short int) port);
	
		while (bind (ssocket, (struct sockaddr *) &server_in, sizeof(struct sockaddr_in)) < 0) {
			printf ("bind: couldn't bind AF_INET socket?\n");
			sleep(5);
		}
		if (listen (ssocket, 0) == -1) {
			printf ("listen: listen call failed\n");
			exit(-2);
		}
		printf ("\nWaiting for debug server to connect.\n");
#ifdef WIN32
		fd = accept (ssocket, (struct sockaddr *) &client_in, NULL);
		if (fd == -1) {
			printf ("accept: %d\n", WSAGetLastError());
			exit(-3);
		}
#else
		fd = accept (ssocket, (struct sockaddr *) &client_in, &client_in_len);
		if (fd == -1) {
			perror ("accept");
			exit(-3);
		}
#endif
		close (ssocket);

		iaddr = &client_in.sin_addr;
		printf ("Connect\n");
		while ((buffer = fd_read_line (fd, &cxt, FD_RL_SOCKET)) > 0) {

			if (buffer[0] == '?') {
				printf ("(%s) ", &buffer[1]);
				fflush(stdout);
				if ((cmd = fd_read_line (0, &std_in, FD_RL_FILE))) {
					if (send (fd, cmd, strlen(cmd), MSG_NOSIGNAL) == -1) {
						break;
					}
					if (send (fd, "\n", 1, MSG_NOSIGNAL) == -1) {
						break;
					}
					if (strcmp (cmd, "quit") == 0) {
						break;
					}
				}
			} else if (buffer[0] == '-') {
				printf ("%s\n", &buffer[8]);
			} else if (buffer[0] != '+') {
				printf ("%s\n", buffer);
			}
		}
		printf ("Disconnect\n\n");
		close(fd);

		/* Sleep some time to reset the TCP/IP connection */
		sleep(1);
	}
}
