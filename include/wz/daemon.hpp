
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __WZ_DAEMON_HPP__
#define __WZ_DAEMON_HPP__

#include <signal.h>
#include <wz/config.hpp>
#include <wz/listen.hpp>
#include <wz/talker.hpp>
#include <wz/worker.hpp>

extern volatile sig_atomic_t doRestart;
extern volatile sig_atomic_t doQuit;
extern volatile sig_atomic_t myPID;

#ifdef MODEL_FREEBSD_KQUEUE
void watchEvent(int fd, int filter, int flags, u_int fflags, int data);
#endif

#ifdef MODEL_LINUX_EPOLL
void watchEvent(int fd, int events, int op);
#endif

#endif /* __WZ_DAEMON_HPP__ */
