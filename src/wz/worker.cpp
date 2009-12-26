
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <dlfcn.h>
#include <stdarg.h>
#include <wz/daemon.hpp>

wz_worker worker;
wz_server server;

wz_server:: wz_server() {}
wz_server::~wz_server() {}

int wz_server::log_message(int lev, const char *fmt, ...)
{
    char lstr [4096];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lstr, 4096, fmt, ap);

    wz_log_message(WZ_ERR, lev, "PLG: %s", lstr);

    return 0;
}

int wz_server::get_curtime()
{
    return time(NULL);
}

typedef wz_plugin *(* wz_get_plugin_fp)(wz_server *srv);

wz_worker:: wz_worker() {}
wz_worker::~wz_worker() { unload(); }

void wz_worker::doload()
{
    for (std::list<wz_config_plugin>::iterator pd_it = cfg->r.plugins.begin();
            pd_it != cfg->r.plugins.end(); ++pd_it)
    {
        doload_plugin(*pd_it);
    }
}

void wz_worker::unload()
{
    for (wz_plugins_t::iterator pl_it = plugins.begin();
            pl_it != plugins.end(); ++pl_it)
    {
        delete *pl_it; /* plugin is not allocated in constructor */
    }

    plugins.clear();

    for (wz_modules_t::iterator lm_it = modules.begin();
            lm_it != modules.end(); ++lm_it)
    {
        dlclose(lm_it->second);
    }

    modules.clear();
}

void wz_worker::work(const wz_request *in, wz_response *out)
{
    for (wz_plugins_t::iterator pl_it = plugins.begin();
            pl_it != plugins.end(); ++pl_it)
    {
        if (0 != out->status)
        {
            break;
        }

        (*pl_it)->work(in, out);
    }
}

void wz_worker::idle()
{
    for (wz_plugins_t::iterator pl_it = plugins.begin();
            pl_it != plugins.end(); ++pl_it)
    {
        (*pl_it)->idle();
    }
}

void wz_worker::doload_plugin(const wz_config_plugin &p_cfg)
{
    wz_modules_t::const_iterator lm_it = modules.find(p_cfg.path);

    void *mylib = NULL;
    wz_plugin *myplugin = NULL;

    union {
        void *data;
        wz_get_plugin_fp symb;
    } pdata;

    if (lm_it != modules.end())
    {
        mylib = lm_it->second;
    }
    else
    {
        if (NULL == (mylib = dlopen(p_cfg.path.c_str(), 0x00002)))
        {
            throw wz_error ("can't load library %s: %s",
                p_cfg.path.c_str(), dlerror());
        }

        modules[p_cfg.path] = mylib;
    }

    pdata.data = dlsym(mylib, "wz_get_plugin");
    wz_get_plugin_fp get_plugin = pdata.symb;

    const char *error = dlerror();

    if (NULL != error)
    {
        throw wz_error ("can't find symbol wz_get_plugin() in %s: %s",
            p_cfg.path.c_str(), error);
    }

    if (NULL == (myplugin = get_plugin(&server)))
    {
        throw wz_error ("NULL == wz_get_plugin(%p) while %s loading",
            (void *) &server, p_cfg.path.c_str());
    }

    if (0 != myplugin->doset_param(p_cfg.opts.c_str()))
    {
        throw wz_error ("can't load module %s config",
            p_cfg.path.c_str());
    }

    plugins.push_back(myplugin);
}

