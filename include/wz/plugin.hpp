
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_PLUGIN_HPP__
#define __WZ_PLUGIN_HPP__

#include <arpa/inet.h>
#include <netinet/in.h>

#define WZ_HEADER_MAX   32

#define WZ_METHOD_UNDEF 0
#define WZ_METHOD_GET   1
#define WZ_METHOD_POST  2
#define WZ_METHOD_HEAD  3

#define WZ_LOG_ACCESS   0  /* special level for access log       */
#define WZ_LOG_EMERG    1  /* system is unusable                 */
#define WZ_LOG_ALERT    2  /* action must be taken immediately   */
#define WZ_LOG_CRIT     3  /* critical conditions                */
#define WZ_LOG_ERROR    4  /* error conditions                   */
#define WZ_LOG_WARN     5  /* warning conditions                 */
#define WZ_LOG_NOTICE   6  /* normal, but significant, condition */
#define WZ_LOG_INFO     7  /* informational message              */
#define WZ_LOG_DEBUG    8  /* debug-level message                */

struct wz_header
{
    const char *key;
    const char *value;
};

struct wz_request
{
    struct in_addr ip;
    int method;
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
    wz_header headers [WZ_HEADER_MAX];
    char *post_body;
    const char *host;
};

struct wz_response
{
    int status;
    int can_cache;
    const char *content_type;
    const char *location;
    const char *set_cookie;
    const char *set_cookie2;
    const void *body;
    int body_len;
    wz_header headers [WZ_HEADER_MAX];
    int headers_size;
};

class wz_server
{
public:
    wz_server();
    virtual ~wz_server();

    virtual int log_message(int lev, const char *fmt, ...);
    virtual int get_curtime();
};

class wz_plugin
{
public:
    wz_plugin(wz_server * /*srv*/) {}
    virtual ~wz_plugin() {}

    virtual int doset_param(const char * /*param*/) { return 0; }
    virtual int reset_param(const char * /*param*/) { return 0; }
    virtual int unset_param(const char * /*param*/) { return 0; }

    virtual int work(const wz_request *in, wz_response *out) = 0;
    virtual int idle() { return 0; }
};

#endif /* __WZ_PLUGIN_HPP__ */
