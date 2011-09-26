
#ifndef __WZCONFIG_H_XML_34253klmfgkjk43t554g3v45_
#define __WZCONFIG_H_XML_34253klmfgkjk43t554g3v45_

#include <coda/txml.hpp>
#include <map>

#define STRING_SMALL 512

struct wzconfig : public coda::txml_determination_object
{
	struct ROOT : public coda::txml_determination_object
	{
		struct LISTENER : public coda::txml_determination_object
		{
			std::string ip;
			int port;
			int canskip;
			LISTENER()
				: port(80)
				, canskip(0)
			{
			};

			void determine(coda::txml_parser *parser)
			{
				parser->determineMember("ip", ip);
				parser->determineMember("port", port);
				parser->determineMember("canskip", canskip);
			}
		};
		
		struct HTTP_CODES : public coda::txml_determination_object
		{
			struct HTTP_CODE : public coda::txml_determination_object
			{
				int code;
				std::string msg;
				HTTP_CODE()
					: code(0)
				{
				};

				void determine(coda::txml_parser *parser)
				{
					parser->determineMember("code", code);
					parser->determineMember("msg", msg);
				}
			};
			std::list<HTTP_CODE> items;
			void determine(coda::txml_parser *parser)
			{
				parser->determineMember("http-code", items);
			}
		};
		struct PLUGINS : public coda::txml_determination_object
		{
			struct PLUGIN : public coda::txml_determination_object
			{
				std::string name;
				std::string library;
				std::string params;
				void determine(coda::txml_parser *parser)
				{
					parser->determineMember("name", name);
					parser->determineMember("library", library);
					parser->determineMember("params", params);
				}
			};
			std::list<PLUGIN> items;
			void determine(coda::txml_parser *parser)
			{
				parser->determineMember("plugin", items);
			}
		};
		std::list<LISTENER> listeners;
		HTTP_CODES http_codes_list;
		std::map<int, std::string> http_codes;

		std::string log_file_name;
		std::string log_level;
		std::string pid_file_name;
		std::string default_answer;
		std::string error_5xx;
		std::string error_4xx;
		PLUGINS plugins;
		int fd_limit;

		ROOT()
			: fd_limit(100)
		{
		};

		void determine(coda::txml_parser *parser)
		{
			parser->determineMember("default_answer", default_answer);
			parser->determineMember("fd_limit", fd_limit);
			parser->determineMember("log_file_name", log_file_name);
			parser->determineMember("log_level", log_level);
			parser->determineMember("pid_file_name", pid_file_name);
			parser->determineMember("http-codes", http_codes_list);
			parser->determineMember("listener", listeners);
			parser->determineMember("error_5xx", error_5xx);
			parser->determineMember("error_4xx", error_4xx);
			parser->determineMember("plugins", plugins);
		};
		void finalize();
	};
	ROOT r;
	void determine(coda::txml_parser *parser)
	{
		parser->determineMember("wzconfig", r);
	}
};

typedef wzconfig::ROOT::LISTENER listener_config_desc;

extern wzconfig *cfg;

#endif

