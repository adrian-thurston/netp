use NetP "parse.gf";
appid 5;

debug PROXY;

#
# CAP_SYS_ADMIN we need for setns(). CAP_NET_ADMIN we need for listening with
# IP_TRANSPARENT, also for KRING opening.
#
caps CAP_SYS_ADMIN, CAP_NET_ADMIN;

option bool connect: --connect;
option bool makeCa: --make-ca;
option string cert: --cert;
option bool initTls: --init-tls;
option string list host: --host;
option string broker: --broker;
option string netns: --netns;
option bool stashErrors: --stash-errors;
option bool stashAll: --stash-all;

thread Listen;
thread Proxy;
thread Service;

message Shutdown
{
};

message CertGenerated
{
	string host;
};

packet CertGenerated
{
	string host;
};

packet CertGenerated 7;

Main starts Listen;
Main starts Service;

# The listen thread will accept connections, then start a proxy, which is
# responsible for handling both ends of the connection proxy.
Listen starts Proxy;

Proxy sends CertGenerated to Service;

Main sends Shutdown to Listen;
Main sends Shutdown to Service;

Listen sends Shutdown to Proxy;
