#ifndef _LISTENER_H_31415926_
#define _LISTENER_H_31415926_

#include <wzconfig.h>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

class listener
{
	struct in_addr addr;
	int canskip;
	int port;
	int fd;
	int accept_failed;
public:
	listener();
	listener(const listener_config_desc &desc);
	void init();
	void handle();
	void done();
	int get_fd() const { return fd; }
};

extern std::map<int, listener> wz_listeners;

#endif

