#ifndef _server_status_plugin_324535_
#define _server_status_plugin_324535_

#include <wz_handler.h>
#include <string>

class SP_server_status : public Plugin
{
public:
	SP_server_status();
	~SP_server_status();
	int set_param(const char *param) { return 0; };
	void handle(const Request *in, Response *out);
};

#endif

