#pragma once

#include <string>

#include <boost/log/trivial.hpp>
#include <boost/log/attributes.hpp>

/* http://stackoverflow.com/questions/22095667/how-to-log-line-number-of-coder-in-boost-log-2-0 */

namespace faf
{

#define CUSTOM_LOG(logger, sev) \
   BOOST_LOG_STREAM_WITH_PARAMS( \
      (logger), \
         (faf::set_get_attrib("File", faf::path_to_filename(__FILE__))) \
         (faf::set_get_attrib("Function", faf::path_to_filename(__func__))) \
         (faf::set_get_attrib("Line", __LINE__)) \
         (::boost::log::keywords::severity = (boost::log::trivial::sev)) \
   )

// Set attribute and return the new value
template<typename ValueType>
ValueType set_get_attrib(const char* name, ValueType value)
{
   auto attr = boost::log::attribute_cast<boost::log::attributes::mutable_constant<ValueType>>(boost::log::core::get()->get_global_attributes()[name]);
   attr.set(value);
   return attr.get();
}

// Convert file path to only the filename
std::string path_to_filename(std::string path);

boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger();

void logging_init(std::string const& verbosity);
void logging_init_log_file(std::string const& verbosity,
                           std::string const& log_file);

#define FAF_LOG_TRACE CUSTOM_LOG(faf::logger(), trace)
#define FAF_LOG_DEBUG CUSTOM_LOG(faf::logger(), debug)
#define FAF_LOG_INFO CUSTOM_LOG(faf::logger(), info)
#define FAF_LOG_WARN CUSTOM_LOG(faf::logger(), warning)
#define FAF_LOG_ERROR CUSTOM_LOG(faf::logger(), error)

}
