
option long l: -l --long;

thread User;

debug THREAD;
debug IP;
debug TCP;

message Shutdown
{
};

message Hello
{
};

Main starts User;

Main sends Shutdown to User;
Main sends Hello to User;
