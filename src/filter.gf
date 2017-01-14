module filter;

kobj link
{
	attribute store port_add( string port, string side );
	attribute store port_del( string port );

	attribute store ip_add( string ip );
};

attribute store add( string name );
attribute store del( string name );

