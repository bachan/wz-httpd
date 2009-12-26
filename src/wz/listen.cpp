
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <auto/config.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <netinet/tcp.h>
#include <wz/daemon.hpp>

#if defined(MODEL_LINUX_EPOLL)
#include <sys/epoll.h>
#elif defined(MODEL_FREEBSD_KQUEUE)
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>
#else
#error "Unknown model!"
#endif

std::map <int, wz_listen> wz_listens;

wz_listen::wz_listen() : canskip(0), port(80), fd(-1), accept_failed(0)
{
    memset(&addr, 0, sizeof(struct in_addr));
}

wz_listen::wz_listen(const wz_config_listen &l_cfg) : fd(-1), accept_failed(0)
{
    if (l_cfg.ip.empty())
    {
        addr.s_addr = 0;
    }
    else
    {
        inet_aton(l_cfg.ip.c_str(), &addr);
    }

    port = l_cfg.port;
    canskip = l_cfg.canskip;
    backlog = l_cfg.backlog;
}

void wz_listen::init()
{
    if (get_fd() != -1) return;
    wz_log_debug(WZ_ERR, "creating socket for listening %s:%d", inet_ntoa(addr), port);
    int new_fd;
    if ((new_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        wz_log_error(WZ_ERR, "Cannot create socket: %s", strerror(errno));
        if (canskip) return;
        throw wz_error("listener::init(): Cannot create socket.");
    }
    fd = new_fd;

#if 0
    if (fd >= cfg->r.fd_limit)
    {
        close(fd);
        fd = -1;
        if (canskip) return;
        throw wz_error("listener::init(): Too many file descriptors.");
    }
#endif

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
    wz_log_debug(WZ_ERR, "binding socket fd(%d)", fd);
    if (bind(fd, (struct sockaddr*)&sa, sizeof sa) == -1)
    {
        wz_log_error(WZ_ERR, "Cannot bind %s:%d: %s",
            inet_ntoa(addr), port, strerror(errno));
        close(fd);
        fd = -1;
        if (canskip) return;
        throw wz_error("listener::init(): bind() failed.");
    }

    if (listen(fd, backlog) == -1)
    {
        close(fd);
        fd = -1;
        if (canskip) return;
        throw wz_error("Cannot listen");
    }
}

void wz_listen::done()
{
    if (get_fd() != -1)
    {
        close(fd);
        fd = -1;
    }
}

const char *busy_answer = "HTTP/1.0 503 Service Unavailable" CRLF
                          "Server: wz/" WZ_HTTPD_VERSION CRLF
                          "Date: %s" CRLF
                          "Content-type: text/html" CRLF CRLF
                          "%s" CRLF;

void MakeBusyReply(int fd)
{
    char buf[2048];
    char now_str[128];
    time_t now_time;
    time(&now_time);
    strftime(now_str, 128, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now_time));
    int n = snprintf(buf, 2048, busy_answer, now_str, "5xx");
    if (0 > write(fd, buf, n + 1)) { /* void */ }
}

void wz_listen::work()
{
    struct sockaddr_in sa;
    socklen_t lsa;
    int client_fd;
    
    lsa = sizeof(sa);
    if ((client_fd = accept(fd, (struct sockaddr *) &sa, &lsa)) == -1)
    {
        wz_log_error(WZ_ERR, "accept() failed %d, %s", errno, strerror(errno));
        if (accept_failed)
        {
            wz_log_error(WZ_ERR, "Double-fail of accept() for socket %d", fd);
            //doRestart = 1;
            throw wz_error("Double-fail of accept()");
        }
        if (errno == EAGAIN) accept_failed = 1;
        return;
    }
    accept_failed = 0;

//    MakeBusyReply(client_fd);
//    close(client_fd);
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

