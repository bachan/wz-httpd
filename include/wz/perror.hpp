
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __AU_ERROR_HPP__
#define __AU_ERROR_HPP__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <exception>

#ifdef __GNUC__
# define WZ_FORMAT(n, f, e) __attribute__ ((format (n, f, e)))
#else
# define WZ_FORMAT(n, f, e) /* void */
#endif

#define WZ_ERROR_BUFSZ 4096

class wz_error : public std::exception
{
    char msg [WZ_ERROR_BUFSZ];

public:
    wz_error(const char *fmt, ...) throw() WZ_FORMAT(printf, 2, 3)
    {
        va_list ap; va_start(ap, fmt);
        vsnprintf(msg, WZ_ERROR_BUFSZ, fmt, ap);
    }

    wz_error(int errnum, const char *s) throw()
    {
        snprintf(msg, WZ_ERROR_BUFSZ, "%s: %d: %s",
            s, errnum, strerror(errnum));
    }

    virtual ~wz_error() throw() {}
    virtual const char *what() const throw() { return msg; }
};

#endif /* __AU_ERROR_HPP__ */
