#include <plugins/server_status.h>
#include <coda/logger.h>
#include <string.h>

SP_server_status::SP_server_status()
{
}

SP_server_status::~SP_server_status()
{
}

const char *lalala = "server status: OK\n";

void SP_server_status::handle(const Request *in, Response *out)
{
	if (!strncmp(in->uri_path, "/server_status", 14))
	{
		log_notice("%s /server_status", inet_ntoa(in->ip));
		out->status = 200;
		out->body = lalala;
		out->body_len = strlen(lalala);
	} 
}

