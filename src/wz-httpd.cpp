#include <wz-httpd.h>
#include <stdexcept>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include "config.h"

#if defined(MODEL_LINUX_EPOLL)
#include <sys/epoll.h>

void wz_epoll_init();

#elif defined(MODEL_FREEBSD_KQUEUE)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#else
#error "Unknown model!"
#endif

wzconfig *cfg;
int doRestart = 0;
int doQuit = 0;
int doLogRotate = 0;
int myPID = 0;
int doFork = 0;
std::string c_fname;

////////////////////////////////////////////////////////////////////
// Signals stuff
////////////////////////////////////////////////////////////////////

void actionOnUSR1(int sig)
{
	doLogRotate = 1;
}

void actionOnHUP(int sig)
{
	log_notice("HUP received, checking config...");
	try
	{
		wzconfig testcfg;
		testcfg.load_from_file(c_fname.c_str());
		log_notice("config ok, restarting...");
		doRestart = 1;
	}
	catch (const std::exception& e)
	{
		log_notice("Did NOT restart, because there are errors: %s", e.what());
	}
}

void actionOnTERM(int sig)
{
	log_notice("TERM received!");
	doRestart = 1;
	doQuit = 1;
}

void actionOnSEGV(int sig)
{
	log_notice("%d fatal signal received!", sig);
	exit(0);
}

void wzconfig::ROOT::finalize()
{
	std::list<HTTP_CODES::HTTP_CODE>::iterator it;
	for (it = http_codes_list.items.begin(); it != http_codes_list.items.end(); it++)
	{
		http_codes[it->code] = it->msg;
	}
	http_codes_list.items.clear();
}

void wz_prefork_init(int argc, char **argv)
{
	if (argc < 2) throw std::logic_error("Usage: wz-httpd [--no-daemon] config.xml");

	c_fname = argv[argc - 1];
	log_info("going to read config: %s", c_fname.c_str());
	cfg->load_from_file(c_fname.c_str());

	if (strcmp("--no-daemon", argv[1]))
	{
		if (cfg->r.log_file_name.empty())
		{
			throw std::logic_error("<log_file_name> is not set in config");
		}
		log_info("now logging to '%s'", cfg->r.log_file_name.c_str());
		log_create(cfg->r.log_file_name.c_str(), log_levels(cfg->r.log_level.c_str()));
		doFork = 1;
	}

	cfg->r.finalize();
}

void wz_postfork_init()
{
	// setting limits
	log_debug("setting fd_limit = %d", cfg->r.fd_limit);
	struct rlimit rl;
	rl.rlim_cur = cfg->r.fd_limit;
	rl.rlim_max = cfg->r.fd_limit;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
	{
		log_error("setrlimit() failed, but it is ok. Errorcode is: %s", strerror(errno));
	}

	// plugins load
	plugins.load_plugins();

	// if no listeners in config, put the default on 80 port
	if (cfg->r.listeners.empty())
	{
		listener_config_desc lcd;
		cfg->r.listeners.push_back(lcd);
	}
	log_info("config file %s was read", c_fname.c_str());

#if defined(MODEL_LINUX_EPOLL)
	wz_epoll_init();
#endif
	
	log_debug("creating listeners");

	std::list<listener_config_desc>::iterator li;
	for (li = cfg->r.listeners.begin(); li != cfg->r.listeners.end(); li++)
	{
		listener l(*li);
		l.init();
		int l_fd = l.get_fd();
		if (l_fd) wz_listeners[l_fd] = l;
	}
	log_debug("created listeners: %d", wz_listeners.size());
	signal(SIGHUP, actionOnHUP);
	signal(SIGTERM, actionOnTERM);
	signal(SIGINT, actionOnTERM);
	signal(SIGSEGV, actionOnSEGV);
	signal(SIGPIPE, SIG_IGN);

	if (!cfg->r.pid_file_name.empty())
	{
		log_info("saving pid=%d to file %s", getpid(), cfg->r.pid_file_name.c_str());
		FILE *pid_f = fopen(cfg->r.pid_file_name.c_str(), "w+");
		if (pid_f != (FILE*)(0)) // here was a great bug: (FILE*)(-1)
		{
			fprintf(pid_f, "%d", getpid());
			fclose(pid_f);
		}
		else
		{
			log_error("error saving pid: %s", strerror(errno));
		}
	}
}

void handle_event(int fd)
{
	std::map<int, listener>::iterator li;
	li = wz_listeners.find(fd);
	if (li != wz_listeners.end())
	{
		log_debug("listener %d received signal", fd);
		li->second.handle();
		return;
	}
	std::map<int, talker>::iterator ri;
	ri = wz_talkers.find(fd);
	if (ri != wz_talkers.end())
	{
		log_debug("talker %d received signal", fd);
		ri->second.handle();
		return;
	}

	log_debug("non-existent fd = %d with signal", fd);
}

time_t clean_talkers()
{
	time_t t = time(0);
	std::map<int, talker>::iterator ri;
	for (ri = wz_talkers.begin(); ri != wz_talkers.end(); ri++)
	{
		if (ri->second.is_timeout(t))
		{
			log_debug("timeout on socket %d", ri->first);
			ri->second.reset();
		}
	}
	return t;
}

#ifdef MODEL_FREEBSD_KQUEUE

int kQueue = 0;
int nChanges = 0;
int aChanges = 0;
struct kevent *kChanges = 0;

void watchEvent(int fd, int filter, int flags, u_int fflags, int data)
{
//	message(0, "doing watch event (%d, %d, %d, %d, %d)", fd, filter, flags, fflags, data);

	if (nChanges == aChanges)
	{
		if (nChanges)
		{
			aChanges <<= 1;
		} else aChanges = 32;

		kChanges = (struct kevent*) realloc(kChanges, aChanges * sizeof(struct kevent));
	}

	EV_SET(kChanges + nChanges, fd, filter, flags, fflags, data, NULL);
	nChanges++;
}

void wz_work()
{
	time_t last_clean_time = time(0);

	kQueue = kqueue();
	if (kQueue == -1)
	{
		log_error("Error creating kqueue: %s", strerror(errno));
		throw std::logic_error("error creating kqueue");
	}

	while (!doQuit && !doRestart)
	{
		struct timespec timeout = {5, 0};
		int r;

		r = kevent(kQueue, kChanges, nChanges, kChanges, aChanges, &timeout);
		nChanges = 0;

		if (r > 0)
		{
			struct kevent* ke = kChanges;
			int i;
			for (i = 0; i < r; i++, ke++)
			{
				if (ke->flags & EV_EOF)
				{
					watchEvent(ke->ident, EVFILT_READ, EV_DELETE, 0, 0);
					close(ke->ident);
					continue;
				}
				handle_event(ke->ident);
			}
		}
		else
		{
			if (r < 0 && errno != EINTR && errno != EAGAIN)
			{
				log_error("Strange error %s, %d", strerror(errno), errno);
			}
		}

		if (time(0) - last_clean_time > 5)
		{
			last_clean_time = clean_talkers();
			plugins.idle();
		}
	}

}

#endif

#ifdef MODEL_LINUX_EPOLL

#define EPOLL_NUM_FDS_HINT 10000
#define EPOLL_MAX_EVENTS   1000

int epollFD = 0;

void watchEvent(int fd, int events, int op)
{
	struct epoll_event evt;
	memset(&evt, 0, sizeof(evt)); // just in case
	evt.events = events;
	evt.data.fd = fd;
	int r = epoll_ctl(epollFD, op, fd, &evt);
	if (r == -1)
	{
		log_error("Error calling epoll_ctl: %s", strerror(errno));
	}
}

void wz_epoll_init()
{
	epollFD = epoll_create(EPOLL_NUM_FDS_HINT);
	if (epollFD == -1)
	{
		log_error("Error creating epoll: %s", strerror(errno));
		throw std::logic_error("error creating epoll");
	}
}

void wz_work()
{
	static struct epoll_event events[EPOLL_MAX_EVENTS];
	time_t last_clean_time = time(0);

	
	while (!doQuit && !doRestart)
	{
		int timeout_msec = 5000;
		int r;

		r = epoll_wait(epollFD, events, EPOLL_MAX_EVENTS, timeout_msec);

		if (r > 0)
		{
			struct epoll_event* evt = &events[0];
			for (int i = 0; i < r; ++i, ++evt)
			{
				if (evt->events & (EPOLLERR|EPOLLHUP))
				{
					// Looks like the handle gets automatically removed
					// on EPOLLERR
					//watchEvent(evt->data.fd, 0, EPOLL_CTL_DEL);
					close(evt->data.fd);
					continue;
				}
				if (evt->events & EPOLLIN) {
					// Note: epoll doesn't (usually) return EPOLLHUP)
					// when partner closes connection.
					char buf[1];
					ssize_t recv_ret = recv(evt->data.fd, buf, 1, MSG_PEEK);
					if (0 == recv_ret)
					{
						watchEvent(evt->data.fd, 0, EPOLL_CTL_DEL);
						close(evt->data.fd);
						continue;
					}
				}
				
				handle_event(evt->data.fd);
			}
		} else {
			if (r < 0 && errno != EINTR && errno != EAGAIN)
			{
				log_error("Strange error %s, %d", strerror(errno), errno);
			}
		}

		if (time(0) - last_clean_time > 5)
		{
			last_clean_time = clean_talkers();
			plugins.idle();
		}
	}

}

#endif

void wz_shutdown()
{
	log_notice("shutting down...");
	plugins.unload_plugins();

	std::map<int, listener>::iterator li;
	for (li = wz_listeners.begin(); li != wz_listeners.end(); li++)
	{
		li->second.done();
	}
	std::map<int, talker>::iterator ri;
	for (ri = wz_talkers.begin(); ri != wz_talkers.end(); ri++)
	{
		ri->second.done();
	}
	wz_listeners.clear();
	wz_talkers.clear();

#if defined(MODEL_FREEBSD_KQUEUE)
	free(kChanges);
	kChanges = 0;
	nChanges = aChanges = 0;
#elif defined(MODEL_LINUX_EPOLL)
	close(epollFD);
	epollFD = 0;
#else
#error "Unknown model!"
#endif

	if (!cfg->r.pid_file_name.empty() && -1 == unlink(cfg->r.pid_file_name.c_str()))
	{
		log_warn("can't remove pid_file: '%s'", cfg->r.pid_file_name.c_str());
	}
}

char *getStatusLine(int code);

int main(int argc, char **argv)
{
//	if (fork()) exit(0);
	myPID = getpid();
	log_thread_name_set("WZ");

	try
	{
		while (!doQuit)
		{
			cfg = new wzconfig();
			wz_prefork_init(argc, argv);

			if (doFork)
			{
				if (fork()) exit(0);
				myPID = getpid();
			}

			wz_postfork_init();
			log_notice("init ok, starting work");

			doRestart = 0;
			wz_work();
			wz_shutdown();
			delete cfg;
		}
	}
	catch (const std::exception& e)
	{
		log_emerg("Fatal exception: %s", e.what());
	}

	log_notice("wz-httpd stop.");
	return 0;
}

