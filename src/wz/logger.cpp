
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <map>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <wz/logger.hpp>

#define BUFSZ 4096

#define CKNN(expr)  if (0 != (expr)) return -1;

#define DATE(buf, sz, ntm) snprintf(buf, sz, "%04u/%02u/%02u %02u:%02u:%02u", ntm.tm_year + 1900, ntm.tm_mon + 1, ntm.tm_mday, ntm.tm_hour, ntm.tm_min, ntm.tm_sec)
#define LEVL(buf, sz, lev) snprintf(buf, sz, " %*.s[%s] *", 6 - log_lvs[lev].len, "", log_lvs[lev].data)
#define MESG(buf, sz, msg) snprintf(buf, sz, ": %s\n", msg)

#define RDOP(oper)  /* void */
#define WROP(oper)  /* void */
#define LSET(id, log)  log_data &(log) = logs[id];
#define LGET(id, log)  log_data &(log) = logs[id];
#define MGET(id, log)  msg_data &(msg) = logs[id].msg;
#define SELF(buf, sz) 0

struct lev_data
{
    const char *data;
    int len;
};

struct msg_data
{
    int lv;
    int fd;
};

struct log_data
{
    msg_data msg;
    char fn [BUFSZ];

public:
    log_data()
    {
        msg.lv = -1;
        msg.fd = -1;

        *fn = 0;
    }
};

typedef std::map<int, log_data> log_map;

static log_map logs;

static lev_data log_lvs [] = 
{
    { "access" , sizeof("access") - 1 },  /* WZ_LOG_ACCESS */
    { "emerg"  , sizeof("emerg" ) - 1 },  /* WZ_LOG_EMERG  */
    { "alert"  , sizeof("alert" ) - 1 },  /* WZ_LOG_ALERT  */
    { "crit"   , sizeof("crit"  ) - 1 },  /* WZ_LOG_CRIT   */
    { "error"  , sizeof("error" ) - 1 },  /* WZ_LOG_ERROR  */
    { "warn"   , sizeof("warn"  ) - 1 },  /* WZ_LOG_WARN   */
    { "notice" , sizeof("notice") - 1 },  /* WZ_LOG_NOTICE */
    { "info"   , sizeof("info"  ) - 1 },  /* WZ_LOG_INFO   */
    { "debug"  , sizeof("debug" ) - 1 },  /* WZ_LOG_DEBUG  */
    {  NULL    ,                    0 },  /* ... */
};

/* aux functions */

static int logwhat(const char *s, size_t len)
{
    for (int lv = 0; log_lvs[lv].data; ++lv)
    {
        if (0 == strncasecmp(s, log_lvs[lv].data, len))
        {
            return lv;
        }
    }

    return -1;
}

static int logpath(char *path)
{
    char *p = path + 1;
    char *e = path + strlen(path);

    for (; p < e; ++p)
    {
        if ('/' != *p) continue;

        char c = *p;
        *p = 0;

        int rc = mkdir(path, 0777);
        *p = c;

        if (0 != rc && EEXIST != errno) return -1;
    }

    return 0;
}

static int logexit(log_data &log)
{
    if (-1 != log.msg.fd && 0 != close(log.msg.fd))
    {
        return -1;
    }

    return 0;
}

static int logopen(log_data &log)
{
    if (0 != logexit(log))
    {
        return -1;
    }

    if (WZ_LOG_ACCESS > log.msg.lv || WZ_LOG_DEBUG < log.msg.lv)
    {
        return -1;
    }

    if (0 != logpath(log.fn))
    {
        return -1;
    }

    if (-1 == (log.msg.fd = open(log.fn, O_CREAT|O_APPEND|O_WRONLY, 0644)))
    {
        return -1;
    }

    return 0;
}

/* main */

int wz_log_snum(int id, const char *fn, int lv)
{
    LSET(id, log);

    snprintf(log.fn, BUFSZ, "%s", fn);
    log.msg.lv = lv;

    CKNN(logopen(log));
    WROP(logpass(id, log));

    return 0;
}

int wz_log_sstr(int id, const char *fn, const char *ls)
{
    return wz_log_snum(id, fn, logwhat(ls, strlen(ls)));
}

int wz_log_sacc(int id, const char *fn)
{
    return wz_log_snum(id, fn, WZ_LOG_ACCESS);
}

int wz_log_rotate(int id)
{
    LGET(id, log);
    CKNN(logopen(log));
    WROP(logpass(id, log));

    return 0;
}

int wz_log_close(int id)
{
    LGET(id, log);
    CKNN(logexit(log));
    WROP(logs.erase(id));

    return 0;
}

int wz_log_message(int id, int lv, const char *fmt, ...)
{
    MGET(id, msg);

    if (-1 == msg.fd) return -1;
    if (lv  > msg.lv) return -1;

    char sb [BUFSZ];
    char wb [BUFSZ];

    va_list ap; va_start(ap, fmt);
    vsnprintf(sb, BUFSZ, fmt, ap);

    time_t ts; struct tm nt;
    time(&ts); localtime_r(&ts, &nt);

    /* log */

    int bc = 0;

    bc += DATE(wb + bc, BUFSZ - bc, nt);
    bc += LEVL(wb + bc, BUFSZ - bc, lv);
    bc += SELF(wb + bc, BUFSZ - bc);
    bc += MESG(wb + bc, BUFSZ - bc, sb);

    /* write */

    return (0 <= write(msg.fd, wb, bc)) ? 0 : -1;
}


#if 0

void aux::log_set_thread_name(pthread_t *p, const char *name)
{
    pthread_mutex_lock(&l_mutex);
    thread_names[p ? *p : pthread_self()] = name;
    pthread_mutex_unlock(&l_mutex);
}

---

char thread_id[64];
std::map<pthread_t, std::string>::const_iterator pit = thread_names.find(pthread_self());
if (pit == thread_names.end())
{
    snprintf(thread_id, 64, "%u", (unsigned int)pthread_self());
}
else
{
    snprintf(thread_id, 64, "%s", pit->second.c_str());
}

#endif


