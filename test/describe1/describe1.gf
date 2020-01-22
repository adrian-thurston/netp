appid 51;

struct Record1
{
	tag string t;

	bool     b;
	int      i;
	uint     ui;
	long     l;
	ulong    ul;
	string   s;
	char(10) c;
};

packet StoreMe1
{
	tag string s;
	list<Record1> records;
};

packet StoreMe2
{
	tag string t;
	char(1)  c1;
	char(2)  c2;
	char(5)  c5;
};

packet StoreMe1 1;
packet StoreMe2 2;
