
thread Writer;
thread Reader;

Main starts Writer;
Main starts Reader;

message Shutdown
{

};

Main sends Shutdown to Reader;
