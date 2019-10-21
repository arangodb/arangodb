// Copyright Thomas Kent 2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// 
// This is an example of a program that uses multiple facets of the boost
// program_options library. It will go through different types of config 
// options in a heirarchal manner:
// 1. Default options are set.
// 2. Command line options are set (they override defaults).
// 3. Environment options are set (they override defaults but not command
//    line options).
// 4. Config files specified on the command line are read, if present, in
//    the order specified. (these override defaults but not options from the
//    other steps).
// 5. Default config file (default.cfg) is read, if present (it overrides
//    defaults but not options from the other steps).
// 
// See the bottom of this file for full usage examples
//

#include <boost/program_options.hpp>
namespace po = boost::program_options;
#include <string>
#include <iostream>
#include <map>
#include <stdexcept>
#include <fstream>

const std::string version("1.0");

// Used to exit the program if the help/version option is set
class OptionsExitsProgram : public std::exception
{};

struct GuiOpts
{
   unsigned int width;
   unsigned int height;
};

struct NetworkOpts
{
   std::string address;
   unsigned short port;
};

class OptionsHeirarchy
{
public:
   // The constructor sets up all the various options that will be parsed
   OptionsHeirarchy()
   {
      SetOptions();
   }

   // Parse options runs through the heirarchy doing all the parsing
   void ParseOptions(int argc, char* argv[])
   {
      ParseCommandLine(argc, argv);
      CheckForHelp();
      CheckForVersion();
      ParseEnvironment();
      ParseConfigFiles();
      ParseDefaultConfigFile();
   }

   // Below is the interface to access the data, once ParseOptions has been run
   std::string Path()
   {
      return results["path"].as<std::string>();
   }
   std::string Verbosity()
   {
      return results["verbosity"].as<std::string>();
   }
   std::vector<std::string> IncludePath()
   {
      if (results.count("include-path"))
      {
         return results["include-path"].as<std::vector<std::string>>();
      }
      return std::vector<std::string>();
   }
   std::string MasterFile()
   {
      if (results.count("master-file"))
      {
         return results["master-file"].as<std::string>();
      }
      return "";
   }
   std::vector<std::string> Files()
   {
      if (results.count("file"))
      {
         return results["file"].as<std::vector<std::string>>();
      }
      return std::vector<std::string>();
   }
   bool GUI()
   {
      if (results["run-gui"].as<bool>())
      {
         return true;
      }
      return false;
   }
   GuiOpts GuiValues()
   {
      GuiOpts opts;
      opts.width = results["gui.width"].as<unsigned int>();
      opts.height = results["gui.height"].as<unsigned int>();
      return opts;
   }
   NetworkOpts NetworkValues()
   {
      NetworkOpts opts;
      opts.address = results["network.ip"].as<std::string>();
      opts.port = results["network.port"].as<unsigned short>();
      return opts;
   }

private:
   void SetOptions()
   {
      SetCommandLineOptions();
      SetCommonOptions();
      SetConfigOnlyOptions();
      SetEnvMapping();
   }
   void SetCommandLineOptions()
   {
      command_line_options.add_options()
         ("help,h", "display this help message")
         ("version,v", "show program version")
         ("config,c", po::value<std::vector<std::string>>(),
            "config files to parse (always parses default.cfg)")
         ;
      hidden_command_line_options.add_options()
         ("master-file", po::value<std::string>())
         ("file", po::value<std::vector<std::string>>())
         ;
      positional_options.add("master-file", 1);
      positional_options.add("file", -1);
   }
   void SetCommonOptions()
   {
      common_options.add_options()
         ("path", po::value<std::string>()->default_value(""),
            "the execution path to use (imports from environment if not specified)")
         ("verbosity", po::value<std::string>()->default_value("INFO"),
            "set verbosity: DEBUG, INFO, WARN, ERROR, FATAL")
         ("include-path,I", po::value<std::vector<std::string>>()->composing(),
            "paths to search for include files")
         ("run-gui", po::bool_switch(), "start the GUI")
         ;
   }
   void SetConfigOnlyOptions()
   {
      config_only_options.add_options()
         ("log-dir", po::value<std::string>()->default_value("log"))
         ("gui.height", po::value<unsigned int>()->default_value(100))
         ("gui.width", po::value<unsigned int>()->default_value(100))
         ("network.ip", po::value<std::string>()->default_value("127.0.0.1"))
         ("network.port", po::value<unsigned short>()->default_value(12345))
         ;
      // Run a parser here (with no command line options) to add these defaults into
      // results, this way they will be enabled even if no config files are parsed.
      store(po::command_line_parser(0, 0).options(config_only_options).run(), results);
      notify(results);
   }
   void SetEnvMapping()
   {
      env_to_option["PATH"] = "path";
      env_to_option["EXAMPLE_VERBOSE"] = "verbosity";
   }


   void ParseCommandLine(int argc, char* argv[])
   {
      po::options_description cmd_opts;
      cmd_opts.add(command_line_options).add(hidden_command_line_options).add(common_options);
      store(po::command_line_parser(argc, argv).
         options(cmd_opts).positional(positional_options).run(), results);
      notify(results);
   }
   void CheckForHelp()
   {
      if (results.count("help"))
      {
         PrintHelp();
      }
   }
   void PrintHelp()
   {
      std::cout << "Program Options Example" << std::endl;
      std::cout << "Usage: example [OPTION]... MASTER-FILE [FILE]...\n";
      std::cout << "  or   example [OPTION] --run-gui\n";
      po::options_description help_opts;
      help_opts.add(command_line_options).add(common_options);
      std::cout << help_opts << std::endl;
      throw OptionsExitsProgram();
   }
   void CheckForVersion()
   {
      if (results.count("version"))
      {
         PrintVersion();
      }
   }
   void PrintVersion()
   {
      std::cout << "Program Options Example " << version << std::endl;
      throw OptionsExitsProgram();
   }
   void ParseEnvironment() 
   {
      store(po::parse_environment(common_options,
         // The next two lines are the crazy syntax to use EnvironmentMapper as
         // the lookup function for env->config name conversions
         boost::function1<std::string, std::string>(
         std::bind1st(std::mem_fun(&OptionsHeirarchy::EnvironmentMapper), this))),
         results);
      notify(results);
   }
   std::string EnvironmentMapper(std::string env_var)
   {
      // ensure the env_var is all caps
      std::transform(env_var.begin(), env_var.end(), env_var.begin(), ::toupper);

      auto entry = env_to_option.find(env_var);
      if (entry != env_to_option.end())
      {
         return entry->second;
      }
      return "";
   }
   void ParseConfigFiles()
   {
      if (results.count("config"))
      {
         auto files = results["config"].as<std::vector<std::string>>();
         for (auto file = files.begin(); file != files.end(); file++)
         {
            LoadAConfigFile(*file);
         }
      }
   }
   void LoadAConfigFile(std::string filename)
   {
      bool ALLOW_UNREGISTERED = true;

      po::options_description config_opts;
      config_opts.add(config_only_options).add(common_options);

      std::ifstream cfg_file(filename.c_str());
      if (cfg_file)
      {
         store(parse_config_file(cfg_file, config_opts, ALLOW_UNREGISTERED), results);
         notify(results);
      }
   }
   void ParseDefaultConfigFile()
   {
      LoadAConfigFile("default.cfg");
   }

   std::map<std::string, std::string> env_to_option;
   po::options_description config_only_options;
   po::options_description common_options;
   po::options_description command_line_options;
   po::options_description hidden_command_line_options;
   po::positional_options_description positional_options;
   po::variables_map results;
};


void get_env_options()
{
}

void PrintOptions(OptionsHeirarchy options)
{
   auto path = options.Path();
   if (path.length())
   {
      std::cout << "First 75 chars of the system path: \n";
      std::cout << options.Path().substr(0, 75) << std::endl;
   }

   std::cout << "Verbosity: " << options.Verbosity() << std::endl;
   std::cout << "Include Path:\n";
   auto includePaths = options.IncludePath();
   for (auto path = includePaths.begin(); path != includePaths.end(); path++)
   {
      std::cout << "   " << *path << std::endl;
   }
   std::cout << "Master-File: " << options.MasterFile() << std::endl;
   std::cout << "Additional Files:\n";
   auto files = options.Files();
   for (auto file = files.begin(); file != files.end(); file++)
   {
      std::cout << "   " << *file << std::endl;
   }

   std::cout << "GUI Enabled: " << std::boolalpha << options.GUI() << std::endl;
   if (options.GUI())
   {
      auto gui_values = options.GuiValues();
      std::cout << "GUI Height: " << gui_values.height << std::endl;
      std::cout << "GUI Width: " << gui_values.width << std::endl;
   }

   auto network_values = options.NetworkValues();
   std::cout << "Network Address: " << network_values.address << std::endl;
   std::cout << "Network Port: " << network_values.port << std::endl;
}

int main(int ac, char* av[])
{
   OptionsHeirarchy options;
   try
   {
      options.ParseOptions(ac, av);
      PrintOptions(options);
   }
   catch (OptionsExitsProgram){}

   return 0;
}

/* 
Full Usage Examples
===================

These were run on windows, so some results may show that environment, but 
results should be similar on POSIX platforms.

Help
----
To see the help screen, with the available options just pass the --help (or -h) 
parameter. The program will then exit.

    > example.exe --help
    Program Options Example
    Usage: example [OPTION]... MASTER-FILE [FILE]...
      or   example [OPTION] --run-gui

      -h [ --help ]              display this help message
      -v [ --version ]           show program version
      -c [ --config ] arg        config files to parse (always parses default.cfg)

      --path arg                 the execution path to use (imports from
                                 environment if not specified)
      --verbosity arg (=INFO)    set verbosity: DEBUG, INFO, WARN, ERROR, FATAL
      -I [ --include-path ] arg  paths to search for include files
      --run-gui                  start the GUI

Version is similar to help (--version or -v).

    > example.exe -v
    Program Options Example 1.0

Basics
------
Running without any options will get the default values (path is set from the 
environment):

    > example.exe 
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

We can easily override that environment path with a simple option:

    > example.exe --path a/b/c;d/e/f
    First 75 chars of the system path:
    a/b/c;d/e/f
    Verbosity: INFO
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

You can use a space or equals sign after long options, also backslashes are
treated literally on windows, on POSIX they need to be escaped.

    > example.exe --path=a\b\c\;d\e\\f
    First 75 chars of the system path:
    a\b\c\;d\e\\f
    Verbosity: INFO
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

For short options you can use a space:

    > example.exe -I path/to/includes
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
        path\to\includes
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

Or you can put the option immediately after it:

    > example.exe -Ipath/to/includes
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
        path\to\includes
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

The include path (--include-path or -I) option allows for multiple paths to be 
specified (both on the command line and in config files) and combined into a 
vector for use by the program.

    > example.exe --include-path=a/b/c --include-path d/e/f -I g/h/i -Ij/k/l
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
       a/b/c
       d/e/f
       g/h/i
       j/k/l
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

There are also the option of flags that do not take parameters and just set a 
boolean value to true. In this case, running the gui also causes default values
for the gui to be output to the screen.

    > example.exe --run-gui
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: true
    GUI Height: 100
    GUI Width: 100
    Network Address: 127.0.0.1
    Network Port: 12345

There are also "positional" options at the end of the command line. The first
one specifies the "master" file the others are additional files.

    > example.exe --path=a-path -I an-include master.cpp additional1.cpp additional2.cpp
    First 75 chars of the system path:
    a-path
    Verbosity: INFO
    Include Path:
       an-include
    Master-File: master.cpp
    Additional Files:
       additional1.cpp
       additional2.cpp
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

Environment Variables
---------------------
In addition to the PATH environment variable, it also knows how to read the 
EXAMPLE_VERBOSE environmental variable and use that to set the verbosity
option/

    > set EXAMPLE_VERBOSE=DEBUG
    > example.exe
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: DEBUG
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

However, if the --verboseity flag is also set, it will override the env
variable. This illustrates an important example, the way program_options works,
is that a parser will not override a value that has previously been set by 
another parser. Thus the env parser doesn't override the command line parser.
(We will see this again in config files.) Default values are seperate from this
heirarcy, they only apply if no parser has set the value and it is being read.

    > set EXAMPLE_VERBOSE=DEBUG
    > example.exe --verbosity=WARN
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: WARN
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

(You can unset an environmental variable with an empty set command)
   
    > set EXAMPLE_VERBOSE=
    > example.exe
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345


Config Files
------------
Config files generally follow the [INI file format]
(https://en.wikipedia.org/wiki/INI_file) with a few exceptions. 

Values can be simply added tp options with an equal sign. Here are two include
paths added via the default config file (default.cfg), you can have optional
spaces around the equal sign.

    # You can use comments in a config file
    include-path=first/default/path
    include-path = second/default/path

Results in

    > example.exe
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
       first/default/path
       second/default/path
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 127.0.0.1
    Network Port: 12345

Values can also be in sections of the config file. Again, editing default.cfg

    include-path=first/default/path
    include-path = second/default/path

    [network]
    ip=1.2.3.4
    port=3000

Results in

    > example.exe
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: INFO
    Include Path:
       first/default/path
       second/default/path
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 1.2.3.4
    Network Port: 3000

This example is also setup to allow multiple config files to be specified on
the command line, which are checked before the default.cfg file is read (but
after the environment and command line parsing). Thus we can set the first.cfg
file to contain the following:

    verbosity=ERROR

    [network]
    ip = 5.6.7.8

Results in:

    > example.exe --config first.cfg
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: ERROR
    Include Path:
       first/default/path
       second/default/path
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 5.6.7.8
    Network Port: 3000

But since the config files are read after the command line, setting the
verbosity there causes the value in the file to be ignored.

    > example.exe --config first.cfg --verbosity=WARN
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: WARN
    Include Path:
       first/default/path
       second/default/path
    Master-File:
    Additional Files:
    GUI Enabled: false
    Network Address: 5.6.7.8
    Network Port: 3000

The config files are parsed in the order they are received on the command line.
So adding the second.cfg file:

    verbosity=FATAL
    run-gui=true

    [gui]
    height=720
    width=1280

Results in a combination of all three config files:

    > example.exe --config first.cfg --config second.cfg
    First 75 chars of the system path:
    C:\Program Files (x86)\MSBuild\14.0\bin;C:\Perl\site\bin;C:\Perl\bin;C:\Pro
    Verbosity: ERROR
    Include Path:
       first/default/path
       second/default/path
    Master-File:
    Additional Files:
    GUI Enabled: true
    GUI Height: 720
    GUI Width: 1280
    Network Address: 5.6.7.8
    Network Port: 3000

Incidently the boolean run-gui option could have been set a number of ways 
that all result in the C++ boolean value of true:

    run-gui=true
    run-gui=on
    run-gui=1
    run-gui=yes
    run-gui=

Since run-gui is an option that was set with the bool_switch type, which
forces its use on the command line without a parameter (i.e. --run-gui instead
of --run-gui=true) it can't be given a "false" option, bool_switch values can 
only be turned true. If instead we had a value ("my-switch", po::value<bool>())
that could be set at the command line --my-switch=true or --my-switch=false, or 
any of the other types of boolean keywords true: true, on, 1, yes; 
false: false, off, 0, no. In a config file this could look like:

    my-switch=true
    my-switch=on
    my-switch=1
    my-switch=yes
    my-switch=

    my-switch=false
    my-switch=off
    my-switch=0
    my-switch=no

*/
