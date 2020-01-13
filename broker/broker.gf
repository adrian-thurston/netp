appid 2;

option string external: --external;
option string replay: --replay;

packet Ping
{
};

packet WantId
{
	long wantId;
};

packet SetRetain
{
	long id;
	int retain;
};

packet Ping            1;
packet WantId          2;
packet SetRetain       3;

Main receives WantId;
Main receives Ping;
Main receives SetRetain;
