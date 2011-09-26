/*

   Copyright (c) 2004--2006, Alexander Pankov (pianozoid@rambler.ru)

   wz-httpd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

*/

#ifndef __WZ_HTTPD_H_dflkjhglsvldjkfhd_
#define __WZ_HTTPD_H_dflkjhglsvldjkfhd_

#include <wzconfig.h>
#include <coda/logger.h>
#include <listener.h>
#include <talker.h>
#include <plugin_factory.h>

extern int doRestart;
extern int doQuit;
extern int myPID;

void wz_prefork_init(int argc, char **argv);
void wz_postfork_init();


#ifdef MODEL_FREEBSD_KQUEUE
void watchEvent(int fd, int filter, int flags, u_int fflags, int data);
#endif

#ifdef MODEL_LINUX_EPOLL
void watchEvent(int fd, int events, int op);
#endif

#endif
