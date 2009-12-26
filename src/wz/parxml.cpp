
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <wz/perror.hpp>
#include <wz/parxml.hpp>

#define BUFSZ 4096

void wz_xmlobject::load_from_file(const char *fname)
{
    char buf [BUFSZ];
    FILE *fp;

    wz_xmlparser p (this);
    p.filename = fname;

    if (NULL == (fp = fopen(fname, "r")))
    {
        throw wz_error("Can't open file \"%s\": %d: %s", fname,
            errno, strerror_r(errno, buf, BUFSZ));
    }

    while (!feof(fp))
    {
        size_t sz = fread(buf, sizeof(char), BUFSZ, fp);

        if (0 != ferror(fp))
        {
            throw wz_error("Can't read from file \"%s\": %d: %s", fname,
                errno, strerror_r(errno, buf, BUFSZ));
        }

        p.parse(buf, sz, feof(fp) ? true : false);
    }

    fclose(fp);
}

void wz_xmlobject::load_from_string(const char *s)
{
    wz_xmlparser p (this);
    p.parse(s, strlen(s), true);
}

void cb_chars(void *vThis, const char *szData, int iLength)
{
    ((wz_xmlparser *) vThis)->chars(szData, iLength);
}

void cb_begelt(void *vThis, const char *const szName, const char **pszAttributes)
{
    ((wz_xmlparser *) vThis)->begelt(szName, pszAttributes);
}

void cb_endelt(void *vThis, const char *const szName)
{
    ((wz_xmlparser *) vThis)->endelt(szName);
}

int cb_unkenc(void *vThis, const XML_Char *name, XML_Encoding *info)
{
    return 0;
}

wz_xmlparser::wz_xmlparser(wz_xmlobject *d) : data(d)
{
    p = XML_ParserCreate("UTF-8");

    XML_SetUserData               (p, this);
    XML_SetUnknownEncodingHandler (p, cb_unkenc, NULL);
    XML_SetElementHandler         (p, cb_begelt, cb_endelt);
    XML_SetCharacterDataHandler   (p, cb_chars);
}

wz_xmlparser::~wz_xmlparser()
{
    XML_ParserFree(p);
}

void wz_xmlparser::raise(const char *err)
{
    throw wz_error("XML error (%s): %s, line %d, column %d", filename.c_str(), err,
        (int) XML_GetCurrentLineNumber(p), (int) XML_GetCurrentColumnNumber(p));
}

void wz_xmlparser::chars(const char *szChars, unsigned int iLength)
{
    current_value += std::string(szChars, iLength);
}

void wz_xmlparser::begelt(const char *szName, const char **pszAttributes)
{
    levels.push_back(szName);

    for (int i = 0; pszAttributes[i] && pszAttributes[i + 1]; i += 2)
    {
        levels.push_back(pszAttributes[i]);
        current_value = pszAttributes[i + 1];
        determine();
        levels.pop_back();
    }

    /* 2006-02-02 bugfix for list & vector */

    levels.push_back("");
    current_value = "";
    determine();
    levels.pop_back();

    /* end of bugfix */

    current_value.clear();
}

void wz_xmlparser::endelt(const char *szName)
{
    determine();
    current_value.clear();
    levels.pop_back();
}

void wz_xmlparser::parse(const char *szDataSource, unsigned int iDataLength, bool bIsFinal)
{
    int iFinal = bIsFinal;

    if (XML_STATUS_ERROR == XML_Parse(p, szDataSource, iDataLength, iFinal))
    {
        raise(XML_ErrorString(XML_GetErrorCode(p)));
    }
}

static bool strtobool(const char *s, size_t sz) /* usual on/off switcher -> bool */
{
    if (0 !=      strtol(s, NULL,    10)) return true;
    if (0 == strncasecmp(s, "0",     sz)) return false;

    if (0 == strncasecmp(s, "true",  sz)) return true;
    if (0 == strncasecmp(s, "false", sz)) return false;

    if (0 == strncasecmp(s, "on",    sz)) return true;
    if (0 == strncasecmp(s, "off",   sz)) return false;

    if (0 == strncasecmp(s, "yes",   sz)) return true;
    if (0 == strncasecmp(s, "no",    sz)) return false;

    return false;
}

void wz_xmlparser::determine()
{
    det_iter = levels.begin();
    data->determine(this);
}

void wz_xmlparser::setValue(std::string &var) { var = current_value; }
void wz_xmlparser::setValue(       bool &var) { var = strtobool (current_value.c_str(), current_value.size()); }
void wz_xmlparser::setValue(    int32_t &var) { var = strtol    (current_value.c_str(), NULL, 10); }
void wz_xmlparser::setValue(   uint32_t &var) { var = strtoul   (current_value.c_str(), NULL, 10); }
void wz_xmlparser::setValue(    int64_t &var) { var = strtoll   (current_value.c_str(), NULL, 10); }
void wz_xmlparser::setValue(   uint64_t &var) { var = strtoull  (current_value.c_str(), NULL, 10); }
void wz_xmlparser::setValue(    uint8_t &var) { var = strtoul   (current_value.c_str(), NULL, 10); }
void wz_xmlparser::setValue(      float &var) { var = strtof    (current_value.c_str(), NULL    ); }
void wz_xmlparser::setValue(     double &var) { var = strtod    (current_value.c_str(), NULL    ); }
void wz_xmlparser::setValue(long double &var) { var = strtold   (current_value.c_str(), NULL    ); }

