/*
 * some dead code collected here to turn into something reusable
 */


struct Date
{
	Date()
		: year(0), month(0), day(0) {}

	Date( int year, int month, int day )
		: year(year), month(month), day(day) {}

	time_t toTime() const;

	int year;  /* Current calendar year. */
	int month; /* One-based. */
	int day;   /* One-based. */
};

int daysBetween( const Date &d1, const Date &d2 );
int daysSince( const Date &date );

struct CmpDate
{
	static int compare( const Date &h1, const Date &h2 )
	{
		if ( h1.year < h2.year )
			return -1;
		else if ( h1.year > h2.year )
			return 1;
		else if ( h1.month < h2.month )
			return -1;
		else if ( h1.month > h2.month )
			return 1;
		else if ( h1.day < h2.day )
			return -1;
		else if ( h1.day > h2.day )
			return 1;
		return 0;
	}
};

time_t Summarizer::Date::toTime() const
{
	struct tm lt;

	lt.tm_sec = 0;    /* Seconds (0-60) */
	lt.tm_min = 0;    /* Minutes (0-59) */
	lt.tm_hour = 0;   /* Hours (0-23) */
	lt.tm_mday = day;          /* Day of the month (1-31) */
	lt.tm_mon = month - 1;     /* Month (0-11) */
	lt.tm_year = year - 1900;  /* Year - 1900 */
	lt.tm_wday = 0;   /* Day of the week (0-6, Sunday = 0) */
	lt.tm_yday = 0;   /* Day in the year (0-365, 1 Jan = 0) */
	lt.tm_isdst = -1; /* Daylight saving time, -1 means mktime should determine it. */

	return mktime( &lt );
}

int Summarizer::daysSince( const Date &date )
{
	time_t now = time(0);
	time_t then = date.toTime();

	return ( now - then ) / 86400;
}

int Summarizer::daysBetween( const Date &d1, const Date &d2 )
{
	time_t t1 = d1.toTime();
	time_t t2 = d2.toTime();

	return ( t2 - t1 ) / 86400;
}

std::ostream &operator<<( std::ostream &out, Summarizer::Date &date )
{
	out << std::setfill('0');
	out << std::right << setw(4) << date.year << '-' << setw(2) <<
		date.month << '-' << setw(2) << date.day;
	out << std::setfill(' ');
	return out;
}

void MainThread::parseDate( int &year, int &month, int &day, const unsigned char *date )
{
	char *tyear = strdup( (const char*)date );
	tyear[4] = 0;
	tyear[7] = 0;

	const char *tmonth = tyear + 5;
	const char *tday = tyear + 8;

	year = atoi( tyear );
	month = atoi( tmonth );
	day = atoi( tday );

	free( tyear );
}


