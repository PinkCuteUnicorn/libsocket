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


// oo wrapper around libinetsocket
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
 * @file inetclientdgram.cpp
 * @brief Class for connectable UDP sockets
 *
 * 	This file provides the class inet_dgram_client which is used
 * 	for internet domain UDP client sockets. You think, in UDP
 * 	there is no difference between client and server? This is
 * 	correct, but in libsocket, the difference is that the UDP
 * 	client sockets may be connected and are not explicitly
 * 	bound to somewhere.
 */

#include <libinetsocket.h>
#include <exception.hpp>
#include <inetclientdgram.hpp>

#include <fcntl.h>
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace libsocket {
using std::string;


// Constructors

/**
 * @brief Create normal datagram socket (connectable).
 *
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`.
 */
inet_dgram_client::inet_dgram_client(int proto_osi3, int flags) {
    setup(proto_osi3, flags);
}

/**
 * @brief Create datagram socket and connect it immediately to the given host
 * and port.
 *
 * @param dsthost Remote host name
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_dgram_client::inet_dgram_client(const char* dsthost, const char* dstport,
                                     int proto_osi3, int flags) {
    setup(dsthost, dstport, proto_osi3, flags);
}

/**
 * @brief Create datagram socket and connect it immediately to the given host
 * and port.
 *
 * @param dsthost Remote host name
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_dgram_client::inet_dgram_client(const string& dsthost,
                                     const string& dstport, int proto_osi3,
                                     int flags) {
    setup(dsthost, dstport, proto_osi3, flags);
}

/**
 * @brief Set up normal datagram socket (connectable). [NOT FOR EXTERNAL USE]
 *
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`.
 */
void inet_dgram_client::setup(int proto_osi3, int flags) {
    if (-1 == (sfd = create_inet_dgram_socket(proto_osi3, flags)))
        throw socket_exception(__FILE__, __LINE__,
                               "inet_dgram_client::inet_dgram_client() - Could "
                               "not create inet dgram socket!");
    proto = proto_osi3;

    is_nonblocking = flags & SOCK_NONBLOCK;
}

/**
 * @brief Set up datagram socket and connect it immediately to the given host
 * and port. [NOT FOR EXTERNAL USE]
 *
 * @param dsthost Remote host name
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_dgram_client::setup(const char* dsthost, const char* dstport,
                              int proto_osi3, int flags) {
    // Retrieve address family
    if (proto_osi3 == LIBSOCKET_BOTH) proto_osi3 = get_address_family(dsthost);

    if (-1 == (sfd = create_inet_dgram_socket(proto_osi3, flags)))
        throw socket_exception(__FILE__, __LINE__,
                               "inet_dgram_client::inet_dgram_client() - Could "
                               "not create inet dgram socket!");

    inet_dgram_client::connect(dsthost, dstport);

    proto = proto_osi3;
    is_nonblocking = flags & SOCK_NONBLOCK;
}

/**
 * @brief Set up datagram socket and connect it immediately to the given host
 * and port. [NOT FOR EXTERNAL USE]
 *
 * @param dsthost Remote host name
 * @param dstport Remote port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_dgram_client::setup(const string& dsthost, const string& dstport,
                              int proto_osi3, int flags) {
    setup(dsthost.c_str(), dstport.c_str(), proto_osi3, flags);
}


static inline signed int check_error(int return_value) {
#ifdef VERBOSE
    const char *errbuf;
#endif
    if (return_value < 0) {
#ifdef VERBOSE
        errbuf = strerror(errno);
    debug_write(errbuf);
#endif
        return -1;
    }

    return 0;
}

int _connect_inet_dgram_socket_(int sfd, const char *host, const char *service, std::string& bufferHost, std::string& bufferPort) {
        struct addrinfo *result, *result_check, hint;
        struct sockaddr_storage oldsockaddr;
        socklen_t oldsockaddrlen = sizeof(struct sockaddr_storage);
        int return_value;
#ifdef VERBOSE
        const char *errstring;
#endif

        if (sfd < 0) return -1;

        if (host == NULL) {
            // This does not work on FreeBSD systems. We pretend to disconnect the
            // socket although we don't do so. This is not very severe for the
            // application
            return 0;
        }

        if (-1 == check_error(getsockname(sfd, (struct sockaddr *)&oldsockaddr,
                                          &oldsockaddrlen)))
            return -1;

        if (oldsockaddrlen >
            sizeof(struct sockaddr_storage))  // If getsockname truncated the struct
            return -1;

        memset(&hint, 0, sizeof(struct addrinfo));

        hint.ai_family = ((struct sockaddr_in *)&oldsockaddr)
                ->sin_family;  // AF_INET or AF_INET6 - offset is same
        // at sockaddr_in and sockaddr_in6
        hint.ai_socktype = SOCK_DGRAM;

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
            if (-1 != (return_value = connect(
                    sfd, result_check->ai_addr,
                    result_check->ai_addrlen)))  // connected without error
            {

                struct sockaddr_in server_addr, my_addr;
                // Get my ip address and port
                bzero(&my_addr, sizeof(my_addr));
                socklen_t len = sizeof(my_addr);
                getsockname(sfd, (struct sockaddr *) &my_addr, &len);

                char tmpIP[16]{0};
                inet_ntop(AF_INET, &my_addr.sin_addr, tmpIP, 16);
                bufferHost = tmpIP;
                bufferPort = std::to_string(ntohs(my_addr.sin_port));

                break;
            } else {
                check_error(return_value);
            }
        }

        // We do now have a working (updated) socket connection to our target

        if (result_check == NULL)  // or not?
        {
#ifdef VERBOSE
            debug_write(
            "connect_inet_dgram_socket: Could not connect to any address!\n");
#endif
            freeaddrinfo(result);
            return -1;
        }

        freeaddrinfo(result);

        return 0;
    }



/**
 * @brief Connect datagram socket.
 *
 * Connect a datagram socket to a remote peer so only its packets are received
 * and all data written is sent to it.
 *
 * @param dsthost Destination host
 * @param dstport Destination port
 */
void inet_dgram_client::connect(const char* dsthost, const char* dstport) {
    if (sfd == -1)
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_dgram_client::connect() - Socket has already been closed!",
            false);
    if (-1 == (_connect_inet_dgram_socket_(sfd, dsthost, dstport, hostClient, portClient)))
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_dgram_client::connect() - Could not connect dgram socket! "
            "(Maybe this socket has a wrong address family?)");

    host = dsthost;
    port = dstport;
    connected = true;
}

/**
 * @brief Connect datagram socket.
 *
 * Connect a datagram socket to a remote peer so only its packets are received
 * and all data written is sent to it.
 *
 * @param dsthost Destination host
 * @param dstport Destination port
 */
void inet_dgram_client::connect(const string& dsthost, const string& dstport) {
    if (sfd == -1)
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_dgram_client::connect() - Socket has already been closed!",
            false);
    if (-1 ==
        (_connect_inet_dgram_socket_(sfd, dsthost.c_str(), dstport.c_str(), hostClient, portClient)))
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_dgram_client::connect() - Could not connect dgram socket! "
            "(Maybe this socket has a wrong address family?)");

    host = dsthost;
    port = dstport;
    connected = true;
}

/*
 * @brief Break association to host. Does not close the socket.
 *
 * *Should actually be called 'disconnect'*
 *
 */
void inet_dgram_client::deconnect(void) {
    if (-1 == (_connect_inet_dgram_socket_(sfd, NULL, NULL, hostClient, portClient)))
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_dgram_client::deconnect() - Could not disconnect!");

    connected = false;
    host.clear();
    port.clear();
}
}  // namespace libsocket
