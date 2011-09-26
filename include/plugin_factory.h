
#ifndef _RFifgbDHdbfgbFdfbHDGHDF_
#define _RFifgbDHdbfgbFdfbHDGHDF_

#include <map>

class Plugin_Factory
{
	typedef wzconfig::ROOT::PLUGINS::PLUGIN plugin_desc;
	std::list<Plugin*> items;
	std::map<std::string, void*> loaded_modules;
	void load_plugin(const plugin_desc &p_d);
public:
	void load_plugins();
	void unload_plugins();
	~Plugin_Factory();
	void handle(const Request *in, Response *out);
	void idle();
};

extern Plugin_Factory plugins;

#endif

