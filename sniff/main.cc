#include "main.h"
#include "sniff.h"
#include "service.h"
#include "itq_gen.h"
#include "packet_gen.h"

#include <unistd.h>
#include <signal.h>
#include <netp/module.h>

extern const char *_PIDFILE;


int MainThread::main()
{
	log_message( "starting" );

	for ( OptStringEl *opt = moduleArgs.head; opt != 0; opt = opt->next )
		moduleList.loadModule( opt->data );

	tlsStartup();

	SniffThread *sniffNet = new SniffThread( "r0", SniffThread::Net );
	SniffThread *sniffDecrypt = new SniffThread( "r1", SniffThread::Decrypted );

	sniffNet->compileBpf();
	sniffDecrypt->compileBpf();

	create( sniffNet );
	create( sniffDecrypt );

	ServiceThread *service = new ServiceThread;
	create( service );

	/* Main sending to sniff and service. */
	SendsToSniff *sendsToSniffNet = registerSendsToSniff( sniffNet );
	SendsToSniff *sendsToSniffDecrypt = registerSendsToSniff( sniffDecrypt );
	SendsToService *sendsToService = registerSendsToService( service );

	/* Sniff sending to service. */
	sniffNet->sendsPassthru = sniffNet->registerSendsPassthru( service );
	sniffDecrypt->sendsPassthru = sniffDecrypt->registerSendsPassthru( service );

	signalLoop();

	sendsToSniffNet->openShutdown();
	sendsToSniffNet->send();

	sendsToSniffDecrypt->openShutdown();
	sendsToSniffDecrypt->send();

	sendsToService->openShutdown();
	sendsToService->send();

	join();

	tlsShutdown();

	log_message( "exiting" );

	return 0;
}
