#ifndef PARSING_CONTEXT_H
#define PARSING_CONTEXT_H

#include <string>
#include <util/maybe.h>

namespace captive
{
	namespace util
	{
		namespace config
		{
			class Configuration;
		}
		
		class CommandLine;
		
		namespace cl
		{
			class ParsingContext
			{
				friend class util::CommandLine;
				
			public:
				ParsingContext(util::config::Configuration& cfg, int nr_args, const char **args) : _config(cfg), _current_index(0), _max_index(nr_args), _args(args) { }
				
				inline util::config::Configuration& config() const { return _config; }
				
				const std::string key() const { return std::string(_args[_current_index]); }
				bool value(std::string& v);
				bool value(maybe<std::string>& v);
				
			private:
				util::config::Configuration& _config;
				
				inline bool have_tokens() const { return _current_index < _max_index; }
				
				inline std::string peek_token() const { 
					if (_current_index >= _max_index) return std::string(); 
					return std::string(_args[_current_index]);
				}
				
				inline std::string consume_token() { 
					if (_current_index >= _max_index) return std::string();
					return std::string(_args[_current_index++]); 
				}
				
				int _current_index, _max_index;
				const char **_args;
			};
		}
	}
}

#endif /* PARSING_CONTEXT_H */

