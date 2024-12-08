#pragma once

// C/C++ headers
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <string>

/**
 * @brief From a pointer to a sockaddr, return a pointer to its underlying
 *        internet address (IPv4 or IPv6).
 *
 * @param pSockAddr Pointer to a socket address struct.
 *
 * @return Pointer to the underlying internet address struct.
 */
[[nodiscard]] void* getInetAddr( struct sockaddr* pSockAddr )
{
    if( pSockAddr == NULL )
    {
        return NULL;
    }

    if( pSockAddr->sa_family == AF_INET )
    {
        return &( ( (struct sockaddr_in*)pSockAddr )->sin_addr );
    }

    return &( ( (struct sockaddr_in6*)pSockAddr )->sin6_addr );
}

[[nodiscard]] in_port_t getInetPort( struct sockaddr* pSockAddr )
{
    if( pSockAddr == NULL )
    {
        return 0;
    }

    if( pSockAddr->sa_family == AF_INET )
    {
        return ntohs( ( (struct sockaddr_in*)pSockAddr )->sin_port );
    }

    return ntohs( ( (struct sockaddr_in6*)pSockAddr )->sin6_port );
}

[[nodiscard]] std::string getAddrString( const struct addrinfo* const pAddrInfo )
{
    if( pAddrInfo == NULL )
    {
        fprintf( stderr, "ERROR: NULL address info struct\n" );
        return "";
    }

    const void* const pInetAddr = getInetAddr( pAddrInfo->ai_addr );
    if( pInetAddr == NULL )
    {
        fprintf( stderr, "ERROR: Unable to get network address\n" );
        return "";
    }

    char addrStr[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop( pAddrInfo->ai_family, pInetAddr, addrStr, sizeof( addrStr ) );

    return std::string( addrStr );
}

[[nodiscard]] std::string getAddrString( const struct sockaddr_storage* const pSockAddr )
{
    if( pSockAddr == NULL )
    {
        fprintf( stderr, "ERROR: NULL socket address\n" );
        return "";
    }

    const void* const pInetAddr = getInetAddr( (struct sockaddr*)pSockAddr );
    if( pInetAddr == NULL )
    {
        fprintf( stderr, "ERROR: Unable to get network address\n" );
        return "";
    }

    char addrStr[INET6_ADDRSTRLEN] = { 0 };
    inet_ntop( pSockAddr->ss_family, pInetAddr, addrStr, sizeof( addrStr ) );

    return std::string( addrStr );
}
