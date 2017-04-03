
thread Writer;
thread Reader;

Main starts Writer;
Main starts Reader;

# Readers indicate to main that they have entered the ring they are reading
# from.
message Entered
{

};

Reader sends Entered to Main;

message Shutdown
{

};

Main sends Shutdown to Reader;
