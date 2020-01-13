appid 3;

option bool connect: --connect;
option string broker: --broker;

#
# Wire-leve parsing error reporting.
#
option bool parseReportFailures: --parse-report-failures;
option bool parseReportJson: --parse-report-json;
option bool parseReportHtml: --parse-report-html;
option bool parseReportHttp: --parse-report-http;

#
# Testing options. Passing data to the parser. 
#
option string parseHttpRequest: --parse-http-request;
option string parseHttpResponse: --parse-http-response;

debug PCAP;
debug ETH;
debug IP;
debug TCP;
debug UDP;
debug HTTP;
debug DECR;
debug DNS;
debug KRING;
debug IDENT;
debug PAT;
debug FILE;
debug GZIP;
debug JSON;
debug BLOCK;
debug VPT;
