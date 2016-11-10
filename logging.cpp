#include "logging.h"

#include <iostream>
#include <string>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
//#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>


std::string path_to_filename(std::string path)
{
   return path.substr(path.find_last_of("/\\")+1);
}

boost::log::sources::severity_logger<boost::log::trivial::severity_level>& logger()
{
  static boost::log::sources::severity_logger<boost::log::trivial::severity_level> l;
  return l;
}

void logging_init()
{
  boost::log::core::get()->add_global_attribute("Line", boost::log::attributes::mutable_constant<int>(5));
  boost::log::core::get()->add_global_attribute("File", boost::log::attributes::mutable_constant<std::string>(""));
  boost::log::core::get()->add_global_attribute("Function", boost::log::attributes::mutable_constant<std::string>(""));

  boost::log::add_console_log(
    std::cout,
    boost::log::keywords::format = boost::log::expressions::stream
                                   << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
                                   << ": <" << boost::log::trivial::severity
                                   << "> " << boost::log::expressions::smessage,
    boost::log::keywords::auto_flush = true
  );
  boost::log::add_common_attributes();

  boost::log::core::get()->set_filter (
      boost::log::trivial::severity >= boost::log::trivial::trace
      );

}

void logging_init_log_file(std::string const& log_file)
{
  boost::log::add_file_log (
    boost::log::keywords::file_name = log_file,
    boost::log::keywords::format = (
      boost::log::expressions::stream
        << boost::log::expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d_%H:%M:%S.%f")
        << ": <" << boost::log::trivial::severity << "> "
        << '['   << boost::log::expressions::attr<std::string>("Function") << '@'
                 << boost::log::expressions::attr<std::string>("File")
                 << ':' << boost::log::expressions::attr<int>("Line") << "] "
        << boost::log::expressions::smessage
      ),
    boost::log::keywords::auto_flush = true
    );
}
