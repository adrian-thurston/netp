module shuttle;

kobj link
{
	attribute store port_add( string port, string side );
	attribute store port_del( string port );

	attribute store ip_add( string ip );

	attribute store block( string ip1, long port1, string ip2, long port2 );
};

attribute store add( string name, string ring );
attribute store del( string name );

