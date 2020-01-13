appid 4;
caps CAP_NET_ADMIN;

option bool connect: --connect;
option string broker: --broker;

option bool parseReportFailures: --parse-report-failures;
option bool parseReportJson: --parse-report-json;
option bool parseReportHtml: --parse-report-html;
option bool parseReportHttp: --parse-report-http;

thread Sniff;
thread Service;

message Shutdown
{
};

packet KringRedirect
{
	string ip;
};

packet KringBlock
{
	string addr1;
	string port1;

	string addr2;
	string port2;
};

packet KringRedirect   5;
packet KringBlock      6;

Main starts Sniff;
Main starts Service;

Main sends Shutdown to Sniff;
Main sends Shutdown to Service;

debug PCAP;
debug ETH;
debug IP;
debug TCP;
debug UDP;
debug HTTP;
debug DECR;
debug DNS;
debug KRING;
debug IDENT;
debug PAT;
debug FILE;
debug GZIP;
debug JSON;
debug BLOCK;

debug PAT_DNS;
