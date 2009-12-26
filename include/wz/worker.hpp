
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_WORKER_HPP__
#define __WZ_WORKER_HPP__

#include <map>
#include <wz/plugin.hpp>

typedef std::list <wz_plugin *> wz_plugins_t;
typedef std::map <std::string, void *> wz_modules_t;

class wz_worker
{
    wz_plugins_t plugins;
    wz_modules_t modules;

    void doload_plugin(const wz_config_plugin &p_cfg);
//  void reload_plugin(const wz_config_plugin &p_cfg);
//  void unload_plugin(const wz_config_plugin &p_cfg);

public:
     wz_worker();
    ~wz_worker();

    void doload();
//  void reload();
    void unload();

    void work(const wz_request *in, wz_response *out);
    void idle();
};

extern wz_worker worker;
extern wz_server server;

#endif /* __WZ_WORKER_HPP__ */
