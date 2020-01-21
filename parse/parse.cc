#include "fmt.h"
#include "parse.h"

extern "C" void netp_open()
{

}

std::ostream &operator<<( std::ostream &out, const FmtEthProto &fmt )
{
	out << std::setfill('0') << std::setw(4) << std::hex << ntohs(fmt.proto);
	return out;
}

std::ostream &operator<<( std::ostream &out, const FmtIpProtocol &fmt )
{
	out << std::setfill('0') << std::setw(2) << std::hex << (uint32_t) fmt.protocol;
	return out;
}

std::ostream &operator<<( std::ostream &out, const FmtIpAddrNet &fmt )
{
	uint32_t haddr = ntohl(fmt.addr);
	out << std::dec << 
		( ( haddr >> 24 ) & 0xff ) << '.' <<
		( ( haddr >> 16 ) & 0xff ) << '.' <<
		( ( haddr >>  8 ) & 0xff ) << '.' <<
		( haddr  & 0xff );
	return out;
}

std::ostream &operator<<( std::ostream &out, const FmtIpAddrHost &fmt )
{
	out << std::dec << 
		( ( fmt.addr >> 24 ) & 0xff ) << '.' <<
		( ( fmt.addr >> 16 ) & 0xff ) << '.' <<
		( ( fmt.addr >>  8 ) & 0xff ) << '.' <<
		( fmt.addr  & 0xff );
	return out;
}

std::ostream &operator<<( std::ostream &out, const FmtIpPortNet &fmt )
{
	out << std::dec << (uint32_t) ntohs(fmt.port);
	return out;
}

std::ostream &operator<<( std::ostream &out, const FmtIpPortHost &fmt )
{
	out << std::dec << (uint32_t) fmt.port;
	return out;
}


std::ostream &operator<<( std::ostream &out, const FmtConnection &fmt )
{
	out <<
		FmtIpAddrHost(fmt.connection->h1.addr1) << ':' << FmtIpPortHost(fmt.connection->h1.port1) << " -> " <<
		FmtIpAddrHost(fmt.connection->h1.addr2) << ':' << FmtIpPortHost(fmt.connection->h1.port2);

	return out;
}
