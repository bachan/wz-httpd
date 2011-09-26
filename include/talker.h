#ifndef _TALKER_H_31415926_
#define _TALKER_H_31415926_

#include <wzconfig.h>
#include <wz_handler.h>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>

class talker
{
	struct in_addr ip;
	time_t last_access;
	size_t cur_header;
	int fd;
	enum { R_UNDEFINED, R_READ_REQUEST_LINE, R_READ_HEADER, R_READ_POST_BODY, R_HANDLE, R_REPLY, R_CLOSE } state;
// buffer stuff
	char *buffer;
	char *buffer_cur_read;
	char *buffer_cur_write;
// post body read stuff
	int readed_bytes;
// response & request
	Response *response;
	Request *request;
// private methods
	char *get_line();
	void do_request_line(char *s);
	void do_header_line(char *s);
	void do_handle();
	void do_reply();
public:
	talker();
	void init(int client_fd, const struct in_addr &addr);
	void handle();
	void reset(int dont_close = 0);
	void done();
	bool is_timeout(const time_t &t);
	int get_fd() { return fd; };
};

extern std::map<int, talker> wz_talkers;

#endif

