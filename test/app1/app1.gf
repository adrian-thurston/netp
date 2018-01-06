
option long l: -l --long;
option bool fetch: --fetch;

thread User;
thread Listen;

debug THREAD;
debug IP;
debug TCP;

message Shutdown
{
};

message Hello
{
};

packet BigPacket
{
	string big1;
	long   l1;
	string big2;
	long   l2;
	string big3;
	long   l3;
};


Main starts User;
Main starts Listen;

Main sends Hello to User;

Main sends Shutdown to User;
Main sends Shutdown to Listen;

Listen sends BigPacket;
Main receives BigPacket;

