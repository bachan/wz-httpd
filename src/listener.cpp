#include <wz-httpd.h>
#include <string.h>
#include <coda/logger.h>
#include <stdexcept>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "config.h"

#if defined(MODEL_LINUX_EPOLL)
#include <sys/epoll.h>
#elif defined(MODEL_FREEBSD_KQUEUE)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#else
#error "Unknown model!"
#endif


std::map<int, listener> wz_listeners;

listener::listener() : canskip(0), port(80), fd(-1), accept_failed(0)
{
	memset(&addr, 0, sizeof(struct in_addr));
}

listener::listener(const listener_config_desc &desc) : fd(-1), accept_failed(0)
{
	if (desc.ip.empty())
	{
		addr.s_addr = 0;
	} else inet_aton(desc.ip.c_str(), &addr);
	port = desc.port;
	canskip = desc.canskip;
}

void listener::init() 
{
	if (get_fd() != -1) return;
	log_info("creating socket for listening %s:%d", inet_ntoa(addr), port);
	int new_fd;
	if ((new_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		log_error("Cannot create socket: %s", strerror(errno));
		if (canskip) return;
		throw std::logic_error("listener::init(): Cannot create socket.");
	}
	fd = new_fd;

	if (fd >= cfg->r.fd_limit)
	{
		close(fd);
		fd = -1;
		if (canskip) return;
		throw std::logic_error("listener::init(): Too many file descriptors.");
	}

	//message(0, "configuring socket");

	int onoff = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &onoff, sizeof(onoff));

#if defined(MODEL_LINUX_EPOLL)
	fcntl(fd, F_SETFL, O_NONBLOCK);
	watchEvent(fd, EPOLLIN, EPOLL_CTL_ADD);
#elif defined(MODEL_FREEBSD_KQUEUE)
	watchEvent(fd, EVFILT_READ, EV_ADD, 0, 0);
	fcntl(fd, F_SETFL, O_NONBLOCK);
#else
#error "Unknown model!"
#endif

	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_addr = addr;
	sa.sin_port = htons(port);
	log_info("binding socket fd(%d)", fd);
	if (bind(fd, (struct sockaddr*)&sa, sizeof sa) == -1)
	{
		log_error("Cannot bind %s:%d: %s", inet_ntoa(addr), port, strerror(errno));
		close(fd);
		fd = -1;
		if (canskip) return;
		throw std::logic_error("listener::init(): bind() failed.");
	}

	//message(0, "first listen socket");
	if (listen(fd, 128) == -1)
	{
		close(fd);
		fd = -1;
		if (canskip) return;
		throw std::logic_error("Cannot listen");
	}
}

void listener::done()
{
	if (get_fd() != -1)
	{
		close(fd);
		fd = -1;
	}
}

#define RN "\r\n"
const char *busy_answer =	"HTTP/1.0 503 Service Unavailable" RN
				"Server: wz/" VERSION RN
				"Date: %s" RN
				"Content-type: text/html" RN RN
				"%s" RN;

void MakeBusyReply(int fd)
{
	char buf[2048];
	char now_str[128];
	time_t now_time;
	time(&now_time);
	strftime(now_str, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now_time));
	int n = snprintf(buf, 2048, busy_answer, now_str, cfg->r.error_5xx.c_str());
	write(fd, buf, n + 1);
}

void listener::handle()
{
	struct sockaddr_in sa;
	socklen_t lsa;
	int client_fd;
	
	lsa = sizeof(sa);
	if ((client_fd = accept(fd, (struct sockaddr *) &sa, &lsa)) == -1)
	{
		log_error("accept() failed %d, %s", errno, strerror(errno));
		if (accept_failed)
		{
			log_crit("Double-fail of accept() for socket %d", fd);
	//		doRestart = 1;
			throw std::logic_error("Double-fail of accept()");
		}
		if (errno == EAGAIN) accept_failed = 1;
		return;
	}
	accept_failed = 0;

//	MakeBusyReply(client_fd);
//	close(client_fd);
#if defined(MODEL_LINUX_EPOLL)
	watchEvent(client_fd, EPOLLIN, EPOLL_CTL_ADD);
	fcntl(client_fd, F_SETFL,  O_NONBLOCK);
#elif defined(MODEL_FREEBSD_KQUEUE)
	watchEvent(client_fd, EVFILT_READ, EV_ADD, 0, 0);
	fcntl(client_fd, F_SETFL, O_NONBLOCK);
#else
#error "Unknown model!"
#endif
	wz_talkers[client_fd].init(client_fd, sa.sin_addr);
}

