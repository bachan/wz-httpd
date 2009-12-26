
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <string.h>
#include <wz/plugin.hpp>

class wz_plugin_status : public wz_plugin
{
    wz_server *srv;

public:
     wz_plugin_status(wz_server *cb);
    ~wz_plugin_status();

    int doset_param(const char *param);
    int reset_param(const char *param);
    int unset_param(const char *param);

    int work(const wz_request *in, wz_response *out);
    int idle();
};

extern "C" wz_plugin *wz_get_plugin(wz_server *cb)
{
    return new wz_plugin_status (cb);
}

wz_plugin_status:: wz_plugin_status(wz_server *cb) : wz_plugin(cb), srv(cb) {}
wz_plugin_status::~wz_plugin_status() {}

int wz_plugin_status::doset_param(const char *param)
{
    return 0;
}

int wz_plugin_status::reset_param(const char *param)
{
    return 0;
}

int wz_plugin_status::unset_param(const char *param)
{
    return 0;
}

static const char location [] = "/status";
static const char response [] = "OK\n";

int wz_plugin_status::work(const wz_request *in, wz_response *out)
{
    if (0 == strncmp(in->uri_path, location, sizeof(location) - 1))
    {
        srv->log_message(WZ_LOG_INFO, "%s %s",
            inet_ntoa(in->ip), location);

        out->status = 200;
        out->body = response;
        out->body_len = sizeof(response) - 1;

        return 0;
    }

    return -1;
}

int wz_plugin_status::idle()
{
    return 0;
}

