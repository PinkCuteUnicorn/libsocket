#include <string.h>
#include <memory>
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

/**
 * @file inetserverstream.cpp
 * @brief INET server class
 *
 * 	inet_stream_server provides the TCP server part of libsocket.
 *	It's main function is accept() which returns a pointer to
 *	a dynamically allocated inet_stream (client socket) class which
 *	provides the connection to the client. You may setup the socket
 *	either with the second constructor or with setup()
 */

#include <conf.h>

#include <libinetsocket.h>
#include <exception.hpp>
#include <inetclientstream.hpp>
#include <inetserverstream.hpp>

#include <fcntl.h>
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK O_NONBLOCK
#endif

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


namespace libsocket {
using std::string;

/**
 * @brief Void constructor; don't forget to setup() the socket before use!
 */
inet_stream_server::inet_stream_server(void) {}

/**
 * @brief Set up a server socket.
 *
 * This constructor initializes a server socket for TCP/IP communication.
 *
 * @param bindhost The address the server should listen on
 * @param bindport The port the server should listen on
 * @param proto_osi3 The protocol: `LIBSOCKET_IPv4/LIBSOCKET_IPv6`
 * @param flags Flags for `socket(2)`
 */
inet_stream_server::inet_stream_server(const char* bindhost,
                                       const char* bindport, int proto_osi3,
                                       const Optional& anOptional) {
    setup(bindhost, bindport, proto_osi3, anOptional);
}

/**
 * @brief Set up a server socket.
 *
 * This constructor initializes a server socket for TCP/IP communication.
 *
 * @param bindhost The address the server should listen on
 * @param bindport The port the server should listen on
 * @param proto_osi3 The protocol: `LIBSOCKET_IPv4/LIBSOCKET_IPv6`
 * @param flags Flags for `socket(2)`
 */
inet_stream_server::inet_stream_server(const string& bindhost,
                                       const string& bindport, int proto_osi3,
                                       const Optional& anOptional) {
    setup(bindhost, bindport, proto_osi3, anOptional);
}

/**
 * @brief Set up a server socket.
 *
 * If the zero-argument constructor was used, this method
 * initializes a server socket for TCP/IP communication.
 *
 * @param bindhost The address the server should listen on
 * @param bindport The port the server should listen on
 * @param proto_osi3 The protocol: `LIBSOCKET_IPv4/LIBSOCKET_IPv6`
 * @param flags Flags for `socket(2)`
 */
void inet_stream_server::setup(const char* bindhost, const char* bindport,
                               int proto_osi3, const Optional& anOptional) {
    if (sfd != -1)
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - "
                               "already bound and listening!",
                               false);
    if (bindhost == 0 || bindport == 0)
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - at "
                               "least one bind argument invalid!",
                               false);
    if (-1 == (sfd = create_inet_server_socket(
                   bindhost, bindport, LIBSOCKET_TCP, proto_osi3, anOptional.flags)))
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - "
                               "could not create server socket!");

    host = string(bindhost);
    port = string(bindport);

    is_nonblocking = anOptional.flags & SOCK_NONBLOCK;
}

int __create_inet_server_socket__(const char *bind_addr, const char *bind_port,
                                  char proto_osi4, char proto_osi3, const Optional& anOptional) {
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
 * @brief Set up a server socket.
 *
 * If the zero-argument constructor was used, this method
 * initializes a server socket for TCP/IP communication.
 *
 * @param bindhost The address the server should listen on
 * @param bindport The port the server should listen on
 * @param proto_osi3 The protocol: `LIBSOCKET_IPv4/LIBSOCKET_IPv6`
 * @param flags Flags for `socket(2)`
 */
void inet_stream_server::setup(const string& bindhost, const string& bindport,
                               int proto_osi3, const Optional& anOptional) {
    if (sfd != -1)
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - "
                               "already bound and listening!",
                               false);
    if (bindhost.empty() || bindport.empty())
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - at "
                               "least one bind argument invalid!",
                               false);
    if (-1 ==
        (sfd = __create_inet_server_socket__(bindhost.c_str(), bindport.c_str(),
                                         LIBSOCKET_TCP, proto_osi3, anOptional)))
        throw socket_exception(__FILE__, __LINE__,
                               "inet_stream_server::inet_stream_server() - "
                               "could not create server socket!");

    host = string(bindhost);
    port = string(bindport);

    is_nonblocking = anOptional.flags & SOCK_NONBLOCK;
}


/**
 * @brief Accept a connection and return a socket connected to the client.
 *
 * Waits for a client to connect and returns a pointer to a inet_stream object
 * which can be used to communicate with the client.
 *
 * @param numeric Specifies if the client's parameter (IP address, port) should
 * be delivered numerically in the src_host/src_port parameters.
 * @param accept_flags Flags specified in `accept(2)`
 *
 * @returns A pointer to a connected TCP/IP client socket object.
 */
inet_stream* inet_stream_server::accept(int numeric, int accept_flags) {
    return accept2(numeric, accept_flags).release();
}

/**
 * @brief Accept a connection and return a socket connected to the client.
 *
 * The caller owns the client socket.
 *
 * @param numeric Specifies if the client's parameter (IP address, port) should
 * be delivered numerically in the src_host/src_port parameters.
 * @param accept_flags Flags specified in `accept(2)`
 *
 * @returns An owned pointer to a connected TCP/IP client socket object.
 */
unique_ptr<inet_stream> inet_stream_server::accept2(int numeric,
                                                    int accept_flags) {
    if (sfd < 0)
        throw socket_exception(
            __FILE__, __LINE__,
            "inet_stream_server::accept() - stream server socket is not in "
            "listening state -- please call first setup()!");

    using std::unique_ptr;
    unique_ptr<char[]> src_host(new char[1024]);
    unique_ptr<char[]> src_port(new char[32]);

    memset(src_host.get(), 0, 1024);
    memset(src_port.get(), 0, 32);

    int client_sfd;
    unique_ptr<inet_stream> client(new inet_stream);

    if (-1 == (client_sfd = accept_inet_stream_socket(sfd, src_host.get(), 1023,
                                                      src_port.get(), 31,
                                                      numeric, accept_flags))) {
        if (!is_nonblocking && errno != EWOULDBLOCK) {
            throw socket_exception(
                __FILE__, __LINE__,
                "inet_stream_server::accept() - could not accept new "
                "connection on stream server socket!");
        } else {
            return nullptr;  // Only return NULL but don't throw an exception if
                             // the socket is nonblocking
        }
    }

    client->sfd = client_sfd;
    client->host =
        string(src_host.get());  // these strings are destructed automatically
                                 // when the returned object is deleted.
                                 // (http://stackoverflow.com/a/6256543)
    client->port = string(src_port.get());  //
    client->proto = proto;

    return client;
}

const string& inet_stream_server::getbindhost(void) { return gethost(); }

const string& inet_stream_server::getbindport(void) { return getport(); }
}  // namespace libsocket
