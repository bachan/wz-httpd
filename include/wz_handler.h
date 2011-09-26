
#ifndef _REQUEST_H_DFHDFDHSDFHEFDHDHDGHDF_
#define _REQUEST_H_DFHDFDHSDFHEFDHDHDGHDF_

#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_HEADER_ITEMS 32

struct Request
{
	struct header_item
	{
		const char *key;
		const char *value;
	};
	struct in_addr ip;
	enum { REQUEST_UNDEFINED, REQUEST_GET, REQUEST_POST, REQUEST_HEAD } method;
	int major;
	int minor;
	int keep_alive;
	const char *uri_path;
	const char *uri_params;
	const char *cookie;
	const char *user_agent;
	const char *x_forwarded_for;
	const char *referer;
	const char *content_type;
	int content_len;

	header_item header_items[MAX_HEADER_ITEMS];
	char *post_body;
	const char *host;

	const char *x_real_ip;
};

struct Response
{
	int status;
	int can_cache;
	const char *content_type;
	const char *location;
	const char *set_cookie;
	const char *set_cookie2;
	const void *body;
	int body_len;
};

class Plugin
{
public:
	Plugin() {};
	virtual ~Plugin() {};
	virtual int set_param(const char *param) = 0;
	virtual void handle(const Request *in, Response *out) = 0;
	virtual void idle() {};
};

#endif

