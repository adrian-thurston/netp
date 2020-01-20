appid 51;

struct Record
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

packet StoreMe
{
	tag string s;
	list<Record> records;
};

packet StoreMe 1;
