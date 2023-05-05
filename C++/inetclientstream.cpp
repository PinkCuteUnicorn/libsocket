#include <string.h>
#include <iostream>
#include <string>

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netdb.h>       // getaddrinfo()
#include <netinet/in.h>  // e.g. struct sockaddr_in on OpenBSD
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  // read()/write()

/*
   The committers of the libsocket project, all rights reserved
   (c) 2012, dermesser <lbo@spheniscida.de>

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS “AS IS” AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/**
 * @file inetclientstream.cpp
 * @brief TCP/IP socket class.
 *
 * 	inetclientstream.cpp provides the class inet_stream
 * 	(which should actually be called inet_stream_client).
 * 	This class is used to communicate with TCP servers,
 * 	like HTTP-, SMTP-, POP3-, FTP-, telnet-Servers.
 *
 * 	The I/O abilities are inherited from stream_client_socket.
 */

#include <libinetsocket.h>
#include <exception.hpp>
#include <inetclientstream.hpp>

namespace libsocket {
using std::string;

/// Void constructor; call connect() before using the socket!
inet_stream::inet_stream(void) {}

/**
 * @brief Connecting constructor
 *
 * Creates TCP/IP client socket and connects.
 *
 * @param dsthost Remote host
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_stream::inet_stream(const char* dsthost, const char* dstport,
                         int proto_osi3, int flags) {
    connect(dsthost, dstport, proto_osi3, flags);
}

/**
 * @brief Connecting constructor
 *
 * Creates TCP/IP client socket and connects.
 *
 * @param dsthost Remote host
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_stream::inet_stream(const string& dsthost, const string& dstport,
                         int proto_osi3, int flags) {
    connect(dsthost.c_str(), dstport.c_str(), proto_osi3, flags);
}

static int _create_inet_stream_socket_(const char *host, const char *service,
                                     char proto_osi3, int flags, std::string& bufferHost, std::string& bufferPort) {

    int sfd, return_value;
    struct addrinfo hint, *result, *result_check;
#ifdef VERBOSE
    const char *errstring;
#endif

    if (host == NULL || service == NULL) return -1;

    memset(&hint, 0, sizeof hint);

    // set address family
    switch (proto_osi3) {
        case LIBSOCKET_IPv4:
            hint.ai_family = AF_INET;
            break;
        case LIBSOCKET_IPv6:
            hint.ai_family = AF_INET6;
            break;
        case LIBSOCKET_BOTH:
            hint.ai_family = AF_UNSPEC;
            break;
        default:
            return -1;
    }

    // Transport protocol is TCP
    hint.ai_socktype = SOCK_STREAM;

    if (0 != (return_value = getaddrinfo(host, service, &hint, &result))) {
#ifdef VERBOSE
        errstring = gai_strerror(return_value);
        debug_write(errstring);
#endif
        return -1;
    }

    // As described in "The Linux Programming Interface", Michael Kerrisk 2010,
    // chapter 59.11 (p. 1220ff)

    for (result_check = result; result_check != NULL;
         result_check = result_check->ai_next)  // go through the linked list of
        // struct addrinfo elements
    {
        sfd = ::socket(result_check->ai_family, result_check->ai_socktype | flags,
                     result_check->ai_protocol);

        if (sfd < 0)  // Error!!!
            continue;

        int CON_RES = connect(sfd, result_check->ai_addr,
                              result_check->ai_addrlen);

        struct sockaddr_in server_addr, my_addr;
        // Get my ip address and port
        bzero(&my_addr, sizeof(my_addr));
        socklen_t len = sizeof(my_addr);
        getsockname(sfd, (struct sockaddr *) &my_addr, &len);

        char tmpIP[16]{0};
        inet_ntop(AF_INET, &my_addr.sin_addr, tmpIP, 16);
        bufferHost = tmpIP;
        bufferPort = std::to_string(ntohs(my_addr.sin_port));


        if ((CON_RES != -1) || (CON_RES == -1 && (flags |= SOCK_NONBLOCK) && ((errno == EINPROGRESS) || (errno == EALREADY) || (errno == EINTR))))     // connected without error, or, connected with errno being one of these important states
            break;

        close(sfd);
    }

    // We do now have a working socket STREAM connection to our target

    if (result_check == NULL)  // Have we?
    {
#ifdef VERBOSE
        debug_write(
            "create_inet_stream_socket: Could not connect to any address!\n");
#endif
        int errno_saved = errno;
        close(sfd);
        errno = errno_saved;
        freeaddrinfo(result);
        return -1;
    }
    // Yes :)

    freeaddrinfo(result);

    return sfd;
}


/**
 * @brief Set up socket if not already done.
 *
 * Creates TCP/IP client socket and connects. Fails if the socket is already set
 * up.
 *
 * @param dsthost Remote host
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_stream::connect(const char* dsthost, const char* dstport,
                          int proto_osi3, int flags) {
    if (sfd != -1)
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream::connect() - Already connected!",
                               false);


    sfd = _create_inet_stream_socket_(dsthost, dstport, proto_osi3, flags, hostClient, portClient);

    if (sfd < 0)
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_stream::connect() - Could not create socket");

    host = dsthost;
    port = dstport;

    proto = proto_osi3;

    // New file descriptor, therefore reset shutdown flags
    shut_rd = false;
    shut_wr = false;
}

/**
 * @brief Set up socket if not already done.
 *
 * Creates TCP/IP client socket and connects. Fails if the socket is already set
 * up.
 *
 * @param dsthost Remote host
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_stream::connect(const string& dsthost, const string& dstport,
                          int proto_osi3, int flags) {
    connect(dsthost.c_str(), dstport.c_str(), proto_osi3, flags);
}
}  // namespace libsocket
