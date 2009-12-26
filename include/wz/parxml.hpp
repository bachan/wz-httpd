
/*
 * Copyright (C) 2007-2009, libturglem development team.
 * This file is released under the LGPL.
 *
 */

#ifndef __AU_PARXML_HPP__
#define __AU_PARXML_HPP__

#include <list>
#include <vector>
#include <string>
#include <expat.h>

class wz_xmlparser;

struct wz_xmlobject
{
	virtual ~wz_xmlobject() {}
	virtual void determine(wz_xmlparser *p) = 0;

	void load_from_file(const char *fname);
	void load_from_string(const char *s);
};

class wz_xmlparser
{
	struct level_desc
	{
		std::string key;
		int pushed;

	public:
		level_desc(const char *k) : key(k), pushed(0) {}
	};

	friend class wz_xmlobject;

	XML_Parser p;
	wz_xmlobject *data;

	std::string filename;
	std::string current_value;
	std::vector<level_desc> levels;
	std::vector<level_desc>::iterator det_iter;

	/* parse */

	void raise(const char *err);
	void parse(const char *szDataSource, unsigned int iDataLength, bool bIsFinal = true);

	void determine();
	void setValue(std::string &var);
	void setValue(       bool &var);
	void setValue(    int32_t &var);
	void setValue(   uint32_t &var);
	void setValue(    int64_t &var);
	void setValue(   uint64_t &var);
	void setValue(    uint8_t &var);
    void setValue(      float &var);
    void setValue(     double &var);
    void setValue(long double &var);

	template <typename _T> void setValue(_T &var)
	{
		var.determine(this);
	}

	template <typename _T> void setValue(std::list<_T> &var)
	{
		det_iter--;

		if (!det_iter->pushed)
		{
			det_iter->pushed = 1;
			var.push_back(_T());
		}

		_T &item = var.back();
		det_iter++;
		setValue(item);
	}

	template <typename _T> void setValue(std::vector<_T> &var)
	{
		det_iter--;

		if (!det_iter->pushed)
		{
			det_iter->pushed = 1;
			var.push_back(_T());
		}

		_T &item = var.back();
		det_iter++;
		setValue(item);
	}

public:
	 wz_xmlparser(wz_xmlobject *d);
	~wz_xmlparser();

	template <typename _T> bool determineMember(const std::string &key, _T &var)
	{
		if (levels.end() == det_iter)
		{
			if (!key.empty()) return false;
			setValue(var);
			return true;
		}

		if (key == det_iter->key)
		{
			det_iter++;
			setValue(var);
			det_iter--;
			return true;
		}

		return false;
	}

	void chars(const char *szChars, unsigned int iLength);
	void begelt(const char *szName, const char **pszAttributes);
	void endelt(const char *szName);
};

#endif /* __AU_PARXML_HPP__ */
