
option long l: -l --long;

thread Bare;

message Hello
{
	bool b;
	long l;
	string s;
};

Main starts Bare;

Main sends Hello to Bare;
