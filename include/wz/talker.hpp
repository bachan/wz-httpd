
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_TALKER_HPP__
#define __WZ_TALKER_HPP__

#include <map>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <wz/config.hpp>
#include <wz/plugin.hpp>

class wz_talker
{
    struct in_addr ip;
    time_t last_access;
    size_t cur_header;
    int fd;
    enum { R_UNDEFINED, R_READ_REQUEST_LINE, R_READ_HEADER, R_READ_POST_BODY, R_HANDLE, R_REPLY, R_CLOSE } state;

    /* buffer stuff */

    char *buffer;
    char *buffer_cur_read;
    char *buffer_cur_write;

    /* post body read stuff */

    int readed_bytes;

    /* response & request */

    wz_response *response;
    wz_request  *request;

    /* private methods */

    char *get_line();
    void do_request_line(char *s);
    void do_header_line(char *s);
    void do_handle();
    void do_reply();
public:
    wz_talker();
    void init(int client_fd, const struct in_addr &addr);
    void handle();
    void reset(int dont_close = 0);
    void done();
    bool is_timeout(const time_t &t);
    int get_fd() { return fd; };
};

extern std::map <int, wz_talker> wz_talkers;

#endif /* __WZ_TALKER_HPP__ */
