
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_LISTEN_HPP__
#define __WZ_LISTEN_HPP__

#include <map>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wz/config.hpp>

class wz_listen
{
    struct in_addr addr;
    int canskip;
    int backlog;
    int port;
    int fd;
    int accept_failed;

public:
    wz_listen();
    wz_listen(const wz_config_listen &l_cfg);

    void init();
    void work();
    void done();

    int get_fd() const { return fd; }
};

extern std::map <int, wz_listen> wz_listens;

#endif /* __WZ_LISTEN_HPP__ */
