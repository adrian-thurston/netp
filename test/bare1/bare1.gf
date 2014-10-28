
option long l: -l --long;

thread Bare;

message Shutdown
{
};

Main starts Bare;

Main sends Shutdown to Bare;
