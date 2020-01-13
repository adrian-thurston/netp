#ifndef _NETP_FMT_H
#define _NETP_FMT_H

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <arpa/inet.h>

#include <iomanip>
#include <iostream>

struct Conn;

struct FmtEthProto
{
	FmtEthProto( uint16_t proto )
		: proto(proto) {}
	uint16_t proto;
};

struct FmtIpProtocol
{
	FmtIpProtocol( uint8_t protocol )
		: protocol(protocol) {}
	uint8_t protocol;
};

struct FmtIpAddrNet
{
	FmtIpAddrNet( uint32_t addr )
		: addr(addr) {}
	uint32_t addr;
};

struct FmtIpAddrHost
{
	FmtIpAddrHost( uint32_t addr )
		: addr(addr) {}
	uint32_t addr;
};

struct FmtIpPortNet
{
	FmtIpPortNet( uint16_t port )
		: port(port) {}
	uint16_t port;
};

struct FmtIpPortHost
{
	FmtIpPortHost( uint16_t port )
		: port(port) {}
	uint16_t port;
};


struct FmtConnection
{
	FmtConnection( Conn *connection )
		: connection(connection) {}
	Conn *connection;
};

std::ostream &operator<<( std::ostream &out, const FmtEthProto &fmt );
std::ostream &operator<<( std::ostream &out, const FmtIpProtocol &fmt );
std::ostream &operator<<( std::ostream &out, const FmtIpAddrNet &fmt );
std::ostream &operator<<( std::ostream &out, const FmtIpAddrHost &fmt );
std::ostream &operator<<( std::ostream &out, const FmtIpPortNet &fmt );
std::ostream &operator<<( std::ostream &out, const FmtIpPortHost &fmt );
std::ostream &operator<<( std::ostream &out, const FmtConnection &fmt );

#endif
