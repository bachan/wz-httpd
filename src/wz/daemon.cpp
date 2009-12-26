
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <auto/config.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <inttypes.h>
#include <sys/resource.h>
#include <wz/daemon.hpp>

#if defined(MODEL_LINUX_EPOLL)
#include <sys/epoll.h>
void wz_epoll_init();
#elif defined(MODEL_FREEBSD_KQUEUE)
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#else
#error "Unknown model!"
#endif

typedef struct wz_getopt wz_getopt_t;

struct wz_getopt
{
    const char *cfg_fn;
    const char *pid_fn;

    int no_daemon;
};

static struct option options [] =
{
    { "config-file"  , 1, NULL, 'c' },
    { "pid-file"     , 1, NULL, 'p' },
    { "no-daemon"    , 0, NULL, 'D' },
    { "no-no-daemon" , 0, NULL, 'E' },
    {  NULL          , 0, NULL,  0  },
};

wz_config *cfg;
static wz_getopt_t opts;

volatile sig_atomic_t doRestart = 0;
volatile sig_atomic_t doQuit = 0;
volatile sig_atomic_t wz_pid = 0;

static int wz_getopt_usage(int argc, char **argv)
{
    fprintf(stderr,

        "Usage:                                            \n"
        "  %s -[Dp:]c <path>                               \n"
        "                                                  \n"
        "Options:                                          \n"
        "  -c, --config-file <path> : path to config file  \n"
        "  -p, --pid-file    <path> : path to pid file     \n"
        "  -D, --no-daemon          : do not daemonize     \n"
        "                                                  \n"

    , argv[0]);

    return 0;
}

static int wz_getopt_parse(int argc, char **argv)
{
    int c;
    opterr = 0;

    while (-1 != (c = getopt_long(argc, argv, "c:p:DE", options, NULL)))
    {
        switch (c)
        {
            case 'c': opts.cfg_fn = optarg; break;
            case 'p': opts.pid_fn = optarg; break;

            case 'D': opts.no_daemon = 1; break;
            case 'E': opts.no_daemon = 0; break;

            default:
                return -1;
        }
    }

    return 0;
}

void actionIgnore(int sig)
{
    wz_log_notice(WZ_ERR, "%d received, "
        "ignoring...", sig);
}

void actionOnHUP(int sig)
{
    wz_log_notice(WZ_ERR, "%d received, "
        "checking config...", sig);

    try
    {
        wz_config testcfg;
        testcfg.load_from_file(opts.cfg_fn);
        wz_log_notice(WZ_ERR, "config ok, restarting...");
        doRestart = 1;
    }
    catch (const std::exception &e)
    {
        wz_log_warn(WZ_ERR, "Did NOT restart, because there are errors: %s", e.what());
    }
}

void actionOnUSR1(int sig)
{
    wz_log_notice(WZ_ERR, "%d received, "
        "rotating...", sig);

    wz_log_rotate(WZ_ERR);

    if (!cfg->r.access_log_file_name.empty())
    {
        wz_log_rotate(WZ_ACC);
    }
}

void actionOnTERM(int sig)
{
    wz_log_notice(WZ_ERR, "%d received, "
        "terminating...", sig);

    doRestart = 1;
    doQuit = 1;
}

/* XXX : ну и раз такое дело, сигналить через sigaction */

void wz_init_prefork()
{
    signal(SIGHUP,    actionOnHUP);
    signal(SIGUSR1,   actionOnUSR1);
    signal(SIGUSR2,   actionIgnore);
    signal(SIGINT,    actionOnTERM);
    signal(SIGTERM,   actionOnTERM);
    signal(SIGALRM,   actionIgnore);
    signal(SIGQUIT,   actionIgnore);
    signal(SIGTRAP,   actionIgnore);
    signal(SIGPIPE,   actionIgnore);
    signal(SIGSTKFLT, actionIgnore);
    signal(SIGCONT,   actionIgnore);

    cfg->load_from_file(opts.cfg_fn);

    if (0 != wz_log_sstr(WZ_ERR, cfg->r.log_file_name.c_str(), cfg->r.log_level.c_str()))
    {
        throw wz_error ("can't open log %s for write",
            cfg->r.log_file_name.c_str());
    }

    if (!cfg->r.access_log_file_name.empty())
    {
        if (0 != wz_log_sacc(WZ_ACC, cfg->r.access_log_file_name.c_str()))
        {
            throw wz_error ("can't open access log %s for write",
                cfg->r.access_log_file_name.c_str());
        }
    }

    if (NULL != opts.pid_fn)
    {
        cfg->r.pid_file_name = opts.pid_fn;
    }

    worker.doload(); /* load plugins */

    if (cfg->r.listens.empty()) /* default listen */
    {
        cfg->r.listens.push_back(wz_config_listen());
    }

    wz_log_debug(WZ_ERR, "config file %s was read",
        opts.cfg_fn);
}

void wz_init_pstfork()
{
#if defined(MODEL_LINUX_EPOLL)
    wz_epoll_init();
#endif
    
    wz_log_debug(WZ_ERR, "create listens");

    for (std::list<wz_config_listen>::iterator cl_it = cfg->r.listens.begin();
            cl_it != cfg->r.listens.end(); ++cl_it)
    {
        wz_listen l (*cl_it);
        l.init();
        int l_fd = l.get_fd();
        if (l_fd) wz_listens[l_fd] = l; /* XXX : l_fd == -1 -- ok? */
    }

    wz_log_debug(WZ_ERR, "create listens %"PRIuMAX" ok",
        (uintmax_t) wz_listens.size());

    if (!cfg->r.pid_file_name.empty())
    {
        FILE *pidfn;

        wz_log_notice(WZ_ERR, "saving pid=%d to file %s",
            wz_pid, cfg->r.pid_file_name.c_str());

        pidfn = fopen(cfg->r.pid_file_name.c_str(), "w+");

        if (NULL == pidfn)
        {
            wz_log_warn(WZ_ERR, "error saving pid: %s",
                strerror(errno));
        }
        else
        {
            fprintf(pidfn, "%d\n", wz_pid);
            fclose(pidfn);
        }
    }
}

void handle_event(int fd)
{
    std::map<int, wz_listen>::iterator l_it = wz_listens.find(fd);

    if (l_it != wz_listens.end())
    {
        wz_log_debug(WZ_ERR, "listen %d received signal", fd);
        l_it->second.work();
        return;
    }

    std::map<int, wz_talker>::iterator t_it = wz_talkers.find(fd);

    if (t_it != wz_talkers.end())
    {
        wz_log_debug(WZ_ERR, "talker %d received signal", fd);
        t_it->second.handle();
        return;
    }

    wz_log_debug(WZ_ERR, "non-existent fd = %d with signal", fd);
}

time_t clean_talkers()
{
    time_t t = time(NULL);

    for (std::map<int, wz_talker>::iterator t_it = wz_talkers.begin();
            t_it != wz_talkers.end(); ++t_it)
    {
        if (t_it->second.is_timeout(t))
        {
            wz_log_debug(WZ_ERR, "timeout on socket %d", t_it->first);
            t_it->second.reset();
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
        wz_log_crit(WZ_ERR, "Error creating kqueue: %s", strerror(errno));
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
        } else {
            if (r < 0 && errno != EINTR && errno != EAGAIN)
            {
                wz_log_emerg(WZ_ERR, "Strange error %s, %d", strerror(errno), errno);
            }
        }

        if (time(0) - last_clean_time > 5)
        {
            last_clean_time = clean_talkers();
            worker.idle();
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
    if (r == -1) {
        wz_log_notice(WZ_ERR, "Error calling epoll_ctl: %s", strerror(errno));
    }
}

void wz_epoll_init()
{
    epollFD = epoll_create(EPOLL_NUM_FDS_HINT);
    if (epollFD == -1)
    {
        wz_log_crit(WZ_ERR, "Error creating epoll: %s", strerror(errno));
        throw wz_error ("error creating epoll");
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
                wz_log_emerg(WZ_ERR, "Strange error %s, %d", strerror(errno), errno);
            }
        }

        if (time(0) - last_clean_time > 5)
        {
            last_clean_time = clean_talkers();
            worker.idle();
        }
    }
}

#endif

void wz_shutdown()
{
    wz_log_notice(WZ_ERR, "shutting down...");
    worker.unload();

    for (std::map<int, wz_listen>::iterator l_it = wz_listens.begin();
            l_it != wz_listens.end(); ++l_it)
    {
        l_it->second.done();
    }

    for (std::map<int, wz_talker>::iterator t_it = wz_talkers.begin();
            t_it != wz_talkers.end(); ++t_it)
    {
        t_it->second.done();
    }

    wz_listens.clear();
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
        wz_log_warn(WZ_ERR, "can't remove pid_file: -1 == unlink(%s): %d: %s",
            cfg->r.pid_file_name.c_str(), errno, strerror(errno));
    }
}

int main(int argc, char **argv)
{
    int rc;

    memset(&opts, 0, sizeof(opts));

    if (0 != wz_getopt_parse(argc, argv))
    {
        wz_getopt_usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    if (NULL == opts.cfg_fn)
    {
        wz_getopt_usage(argc, argv);
        exit(EXIT_FAILURE);
    }

    if (0 == opts.no_daemon)
    {
        close( STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        rc = fork();

        if (0 < rc) exit(EXIT_SUCCESS);
        if (0 > rc) exit(EXIT_FAILURE);

        if ((pid_t) -1 == setsid())
        {
            exit(EXIT_FAILURE);
        }
    }

    wz_pid = getpid();

    try
    {
        while (!doQuit)
        {
            cfg = new wz_config ();
            wz_init_prefork();
            wz_init_pstfork();
            wz_log_notice(WZ_ERR, "init ok, starting work");

            doRestart = 0;
            wz_work();
            wz_shutdown();
            delete cfg;
        }
    }
    catch (const std::exception &e)
    {
        fprintf(stderr, "%s: fatal exception, %s\n",
            argv[0], e.what());

        wz_log_emerg(WZ_ERR, "fatal exception, %s",
            e.what());

        exit(EXIT_FAILURE);
    }

    wz_log_notice(WZ_ERR, "stop");

    return 0;
}

