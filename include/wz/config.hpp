
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_CONFIG_HPP__
#define __WZ_CONFIG_HPP__

#include <map>
#include <wz/logger.hpp>
#include <wz/perror.hpp>
#include <wz/parxml.hpp>

#define WZ_ERR 1
#define WZ_ACC 2

#define CRLF "\r\n"

struct wz_config : public wz_xmlobject
{
    struct R : public wz_xmlobject
    {
        struct LISTEN : public wz_xmlobject
        {
            std::string ip;
            int port;
            int canskip;
            int backlog;

            LISTEN ()
            : port(80)
            , canskip(0)
            , backlog(128)
            {}

            void determine(wz_xmlparser *p)
            {
                p->determineMember("ip", ip);
                p->determineMember("port", port);
                p->determineMember("canskip", canskip);
                p->determineMember("backlog", backlog);
            }
        };
        
        struct PLUGIN : public wz_xmlobject
        {
            std::string name;
            std::string path;
            std::string opts;

            void determine(wz_xmlparser *p)
            {
                p->determineMember("name", name);
                p->determineMember("path", path);
                p->determineMember("opts", opts);
            }
        };

        std::string pid_file_name;
        std::string log_file_name;
        std::string log_level;

        std::string access_log_file_name;

        std::list <LISTEN> listens;
        std::list <PLUGIN> plugins;

        void determine(wz_xmlparser *p)
        {
            p->determineMember("pid_file_name", pid_file_name);
            p->determineMember("log_file_name", log_file_name);
            p->determineMember("log_level", log_level);

            p->determineMember("access_log_file_name", access_log_file_name);

            p->determineMember("listen", listens);
            p->determineMember("plugin", plugins);
        }
    };

    R r;

    void determine(wz_xmlparser *p)
    {
        p->determineMember("wzconfig", r);
    }
};

typedef wz_config::R::LISTEN wz_config_listen;
typedef wz_config::R::PLUGIN wz_config_plugin;

extern wz_config *cfg;

#endif /* __WZ_CONFIG_HPP__ */
