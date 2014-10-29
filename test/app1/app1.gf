
option long l: -l --long;

thread User;

message Shutdown
{
};

Main starts User;

Main sends Shutdown to User;
