
option long l: -l --long;

thread User;

debug THREAD;
debug IP;
debug TCP;

message Shutdown
{
};

Main starts User;

Main sends Shutdown to User;
