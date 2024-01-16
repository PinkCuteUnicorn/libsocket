#include <string.h>
#include <string>

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


#include <netdb.h>
#include <iostream>


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


#define LIBSOCKET_BACKLOG \
    128  ///< Linux accepts a backlog value at listen() up to 128


/**
 * @file inetserverdgram.cpp
 * @brief Contains class for creating a bound datagram socket.
 *
 * inet_dgram_server provides nothing more than a constructor
 * which binds the UDP socket to the specified host. Everything
 * other, e.g. the I/O functions like rcvfrom and sndto are
 * inherited from inet_dgram.
 */

#include <libinetsocket.h>
#include <exception.hpp>
#include <inetserverdgram.hpp>

#include <fcntl.h>
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

namespace libsocket {
using std::string;

/**
 * @brief Create datagram socket and bind it.
 *
 * @param host Bind address (Wildcard: "0.0.0.0"/"::")
 * @param port Bind port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_dgram_server::inet_dgram_server(const char* host, const char* port,
                                     int proto_osi3, const OptionalDgram& anOptional) {
    setup(host, port, proto_osi3, anOptional);
}

/**
 * @brief Create datagram socket and bind it.
 *
 * @param host Bind address (Wildcard: "0.0.0.0"/"::")
 * @param port Bind port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
inet_dgram_server::inet_dgram_server(const string& host, const string& port,
                                     int proto_osi3, const OptionalDgram& anOptional) {
    setup(host, port, proto_osi3, anOptional);
}

int __create_inet_server_socket__(const char *bind_addr, const char *bind_port,
                                  char proto_osi4, char proto_osi3, const OptionalDgram& anOptional) {
    int sfd, domain, type, retval;
    struct addrinfo *result, *result_check, hints;
#ifdef VERBOSE
    const char *errstr;
#endif

    // if ( flags != SOCK_NONBLOCK && flags != SOCK_CLOEXEC && flags !=
    // (SOCK_CLOEXEC|SOCK_NONBLOCK) && flags != 0 ) 	return -1;

    if (bind_addr == NULL || bind_port == NULL) return -1;

    switch (proto_osi4) {
        case LIBSOCKET_TCP:
            type = SOCK_STREAM;
            break;
        case LIBSOCKET_UDP:
            type = SOCK_DGRAM;
            break;
        default:
            return -1;
    }
    switch (proto_osi3) {
        case LIBSOCKET_IPv4:
            domain = AF_INET;
            break;
        case LIBSOCKET_IPv6:
            domain = AF_INET6;
            break;
        case LIBSOCKET_BOTH:
            domain = AF_UNSPEC;
            break;
        default:
            return -1;
    }

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_socktype = type;
    hints.ai_family = domain;
    hints.ai_flags = AI_PASSIVE;

    if (0 != (retval = getaddrinfo(bind_addr, bind_port, &hints, &result))) {
#ifdef VERBOSE
        errstr = gai_strerror(retval);
debug_write(errstr);
#endif
        return -1;
    }

    // As described in "The Linux Programming Interface", Michael Kerrisk 2010,
    // chapter 59.11 (p. 1220ff)
    for (result_check = result; result_check != NULL;
         result_check = result_check->ai_next)  // go through the linked list of
        // struct addrinfo elements
    {
        sfd = ::socket(result_check->ai_family, result_check->ai_socktype | anOptional.flags,
                       result_check->ai_protocol);

        if (sfd < 0)  // Error at socket()!!!
            continue;

        for (auto& it : anOptional.sockOptFlags) {
            int val = 1;
            if (setsockopt(sfd, SOL_SOCKET, it, &val, sizeof(int)) < 0)
                continue;
        }

        retval = bind(sfd, result_check->ai_addr,
                      (socklen_t)result_check->ai_addrlen);

        if (retval != 0)  // Error at bind()!!!
        {
            close(sfd);
            continue;
        }

        if (type == LIBSOCKET_TCP) retval = listen(sfd, LIBSOCKET_BACKLOG);

        if (retval == 0)  // If we came until here, there wasn't an error
            // anywhere. It is safe to cancel the loop here
            break;
        else
            close(sfd);
    }

    if (result_check == NULL) {
#ifdef VERBOSE
        debug_write(
"create_inet_server_socket: Could not bind to any address!\n");
#endif
        freeaddrinfo(result);
        return -1;
    }

    // We do now have a working socket on which we may call accept()

    freeaddrinfo(result);

    return sfd;
}


/**
 * @brief Set up socket. **NOT FOR EXTERNAL USE**
 *
 * @param bhost Bind address (Wildcard: "0.0.0.0"/"::")
 * @param bport Bind port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_dgram_server::setup(const char* bhost, const char* bport,
                              int proto_osi3, const OptionalDgram& anOptional) {
    // No separate call to get_address_family()

    if (-1 == (sfd = __create_inet_server_socket__(bhost, bport, LIBSOCKET_UDP,
                                               proto_osi3, anOptional)))
        throw socket_exception(__FILE__, __LINE__,
                               "inet_dgram_server::inet_dgram_server() - could "
                               "not create server socket!");

    host = string(bhost);
    port = string(bport);
    is_nonblocking = anOptional.flags & SOCK_NONBLOCK;
}

/**
 * @brief Set up socket. **NOT FOR EXTERNAL USE**
 *
 * @param bhost Bind address (Wildcard: "0.0.0.0"/"::")
 * @param bport Bind port
 * @param proto_osi3 `LIBSOCKET_IPv4` or `LIBSOCKET_IPv6` or `LIBSOCKET_BOTH`
 * @param flags Flags for `socket(2)`
 */
void inet_dgram_server::setup(const string& bhost, const string& bport,
                              int proto_osi3, const OptionalDgram& anOptional) {
    setup(bhost.c_str(), bport.c_str(), proto_osi3, anOptional);
}
}  // namespace libsocket
