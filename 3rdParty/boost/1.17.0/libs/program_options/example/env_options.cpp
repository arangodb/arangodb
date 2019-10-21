// Copyright Thomas Kent 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <string>
#include <iostream>

std::string mapper(std::string env_var)
{
   // ensure the env_var is all caps
   std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

   if (env_var == "PATH") return "path";
   if (env_var == "EXAMPLE_VERBOSE") return "verbosity";
   return "";
}

void get_env_options()
{
   po::options_description config("Configuration");
   config.add_options()
      ("path", "the execution path")
      ("verbosity", po::value<std::string>()->default_value("INFO"), "set verbosity: DEBUG, INFO, WARN, ERROR, FATAL")
      ;

   po::variables_map vm;
   store(po::parse_environment(config, boost::function1<std::string, std::string>(mapper)), vm);
   notify(vm);

   if (vm.count("path"))
   {
      std::cout << "First 75 chars of the system path: \n";
      std::cout << vm["path"].as<std::string>().substr(0, 75) << std::endl;
   }

   std::cout << "Verbosity: " << vm["verbosity"].as<std::string>() << std::endl;
}

int main(int ac, char* av[])
{
   get_env_options();

   return 0;
}
