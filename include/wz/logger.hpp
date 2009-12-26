
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __AU_LOGGER_HPP__
#define __AU_LOGGER_HPP__

#include <wz/perror.hpp>
#include <wz/plugin.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#define wz_log_access(id, fmt ...)  wz_log_message ((id), WZ_LOG_ACCESS, ##fmt)
#define  wz_log_emerg(id, fmt ...)  wz_log_message ((id), WZ_LOG_EMERG,  ##fmt)
#define  wz_log_alert(id, fmt ...)  wz_log_message ((id), WZ_LOG_ALERT,  ##fmt)
#define   wz_log_crit(id, fmt ...)  wz_log_message ((id), WZ_LOG_CRIT,   ##fmt)
#define  wz_log_error(id, fmt ...)  wz_log_message ((id), WZ_LOG_ERROR,  ##fmt)
#define   wz_log_warn(id, fmt ...)  wz_log_message ((id), WZ_LOG_WARN,   ##fmt)
#define wz_log_notice(id, fmt ...)  wz_log_message ((id), WZ_LOG_NOTICE, ##fmt)
#define   wz_log_info(id, fmt ...)  wz_log_message ((id), WZ_LOG_INFO,   ##fmt)

#ifndef  wz_log_debug
#define  wz_log_debug(id, fmt ...)  wz_log_message ((id), WZ_LOG_DEBUG,  ##fmt)
#endif

int wz_log_snum(int id, const char *fn, int lv);
int wz_log_sstr(int id, const char *fn, const char *ls);
int wz_log_sacc(int id, const char *fn);

int wz_log_message(int id, int lv, const char *fmt, ...)
    WZ_FORMAT(printf, 3, 4);

int wz_log_rotate(int id);
int wz_log_close(int id);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* __WZ_LOGGER_HPP__ */
