/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   option-handler.h
 * Author: s0457958
 *
 * Created on 01 September 2016, 10:10
 */

#ifndef OPTION_HANDLER_H
#define OPTION_HANDLER_H

#include <string>
#include <util/maybe.h>

namespace captive
{
	namespace util
	{
		class CommandLine;
		
		namespace config
		{
			class Configuration;
		}
		
		namespace cl
		{
			class ParsingContext;
			
			namespace OptionRequirement
			{
				enum OptionRequirement
				{
					None,
					Once,
					PermitMultiple,
				};
			}
			
			namespace HandleResult
			{
				enum HandleResult
				{
					OK,
					MissingArgument,
					InvalidArgument
				};
			}
						
			class OptionHandler
			{
				friend class util::CommandLine;
				
			public:
				OptionHandler(OptionRequirement::OptionRequirement option_requirement) 
					: _option_requirement(option_requirement),
					_visited(0) { }
				
				virtual ~OptionHandler() { }
				
				virtual HandleResult::HandleResult handle(util::config::Configuration& config, maybe<std::string> value) const = 0;
				
			protected:
				OptionRequirement::OptionRequirement option_requirement() const { return _option_requirement; }
				
				int visited() const { return _visited; }
				void visit() { _visited++; }
			
			private:
				const OptionRequirement::OptionRequirement _option_requirement;
				int _visited;
			};
			
			class OptionHandlerRegistration
			{
			public:
				OptionHandlerRegistration(std::string tag, OptionHandler& handler);
				
			private:
				std::string _tag;
				const OptionHandler& _handler;
			};
		}
	}
}

#define DEFINE_OPTION_HANDLER(__tag, __name, __oreq) class __name##OptionHandler : public OptionHandler \
{ \
public: \
	__name##OptionHandler(OptionRequirement::OptionRequirement oreq) : OptionHandler(oreq) { } \
	virtual HandleResult::HandleResult handle(captive::util::config::Configuration& config, maybe<std::string> arg) const override; \
}; \
namespace registrations { \
static __name##OptionHandler __option_handler_##__name(__oreq); \
static OptionHandlerRegistration __option_handler__reg__##__name(__tag, __option_handler_##__name); \
} \
HandleResult::HandleResult __name##OptionHandler::handle(captive::util::config::Configuration& config, maybe<std::string> arg) const

#endif /* OPTION_HANDLER_H */

