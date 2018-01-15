// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_USE_WINDOWS_H

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm_ext/push_back.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/process/environment.hpp>
#include <vector>
#include <string>
#include <iterator>
#include <iostream>
#include <cstdlib>
#if defined(BOOST_POSIX_API)
#   include <boost/lexical_cast.hpp>
#   include <boost/iostreams/device/file_descriptor.hpp>
#   include <boost/iostreams/stream.hpp>
#   include <unistd.h>
#elif defined(BOOST_WINDOWS_API)
#   include <windows.h>
#endif


using namespace boost::program_options;

int main(int argc, char *argv[])
{
    options_description desc;
    desc.add_options()
        ("echo-stdout", value<std::string>())
        ("echo-stderr", value<std::string>())
        ("echo-stdout-stderr", value<std::string>())
        ("echo-argv", bool_switch())
        ("exit-code", value<int>())
        ("wait", value<int>())
        ("is-closed-stdin", bool_switch())
        ("is-closed-stdout", bool_switch())
        ("is-closed-stderr", bool_switch())
        ("is-nul-stdin", bool_switch())
        ("is-nul-stdout", bool_switch())
        ("is-nul-stderr", bool_switch())
        ("loop", bool_switch())
        ("prefix", value<std::string>())
        ("prefix-once", value<std::string>())
        ("pwd", bool_switch())
        ("query", value<std::string>())
        ("stdin-to-stdout", bool_switch())
#if defined(BOOST_POSIX_API)
        ("posix-echo-one", value<std::vector<std::string> >()->multitoken())
        ("posix-echo-two", value<std::vector<std::string> >()->multitoken());
#elif defined(BOOST_WINDOWS_API)
        ("windows-print-showwindow", bool_switch())
        ("windows-print-flags", bool_switch());
#endif
    variables_map vm;
    command_line_parser parser(argc, argv);
    store(parser.options(desc).allow_unregistered().run(), vm);
    notify(vm);

    if (vm.count("echo-stdout"))
    {
        std::cout << vm["echo-stdout"].as<std::string>() << std::endl;
    }
    else if (vm.count("echo-stderr"))
    {
        std::cerr << vm["echo-stderr"].as<std::string>() << std::endl;
    }
    else if (vm.count("echo-stdout-stderr"))
    {
        std::cout << vm["echo-stdout-stderr"].as<std::string>() << std::endl;
        std::cerr << vm["echo-stdout-stderr"].as<std::string>() << std::endl;
    }
    else if (vm["echo-argv"].as<bool>())
    {
        std::vector<char*> args(argv+1, argv + argc);
        for (auto & arg : args)
            std::cout << arg << std::endl;
    }
    else if (vm.count("exit-code"))
    {
        return vm["exit-code"].as<int>();
    }
    else if (vm.count("wait"))
    {
        int sec = vm["wait"].as<int>();
#if defined(BOOST_POSIX_API)
        sleep(sec);
#elif defined(BOOST_WINDOWS_API)
        Sleep(sec * 1000);
#endif
    }
    else if (vm["is-closed-stdin"].as<bool>())
    {
        std::string s;
        std::cin >> s;
        return std::cin.eof() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    else if (vm["is-closed-stdout"].as<bool>())
    {
        std::cout << "foo" << std::endl;
        return std::cout.bad() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    else if (vm["is-closed-stderr"].as<bool>())
    {
        std::cerr << "foo" << std::endl;
        return std::cerr.bad() ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    else if (vm["is-nul-stdin"].as<bool>())
    {
#if defined(BOOST_POSIX_API)
        char buffer[1];
        int res = read(STDIN_FILENO, buffer, 1);
        return res != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
#elif defined(BOOST_WINDOWS_API)
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == INVALID_HANDLE_VALUE)
            return EXIT_FAILURE;
        char buffer[1];
        DWORD read;
        BOOL res = ReadFile(h, buffer, 1, &read, NULL);
        CloseHandle(h);
        return res ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
    }
    else if (vm["is-nul-stdout"].as<bool>())
    {
        std::cout << "foo" << std::endl;
        return std::cout.bad() ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    else if (vm["is-nul-stderr"].as<bool>())
    {
        std::cerr << "foo" << std::endl;
        return std::cerr.bad() ? EXIT_FAILURE : EXIT_SUCCESS;
    }
    else if (vm["loop"].as<bool>())
    {
        while (true);
    }
    else if (vm.count("prefix"))
    {
        std::string line;
        while (std::getline(std::cin, line))
            std::cout << vm["prefix"].as<std::string>() << line << std::endl;
    }
    else if (vm.count("prefix-once"))
    {
        std::string line;

        std::getline(std::cin, line);

        std::cout << vm["prefix-once"].as<std::string>() << line << std::endl;
    }
    else if (vm["pwd"].as<bool>())
    {
        std::cout << boost::filesystem::current_path().string() << std::endl;
    }
    else if (vm.count("query"))
    {
        auto key = vm["query"].as<std::string>();
        auto env = boost::this_process::environment();
        auto val = env[key];
        if (val.empty())
            std::cout << "************** empty environment **************" << std::endl;
        else
            std::cout << val.to_string() << std::endl;
    }
    else if (vm["stdin-to-stdout"].as<bool>())
    {
        char ch;
        while (std::cin >> std::noskipws >> ch)
            std::cout << ch << std::flush;
    }
#if defined(BOOST_POSIX_API)
    else if (vm.count("posix-echo-one"))
    {
        using namespace boost::iostreams;
        std::vector<std::string> v = vm["posix-echo-one"].as<std::vector<std::string> >();
        int fd = boost::lexical_cast<int>(v[0]);
        file_descriptor_sink sink(fd, close_handle);
        stream<file_descriptor_sink> os(sink);
        os << v[1] << std::endl;
    }
    else if (vm.count("posix-echo-two"))
    {
        using namespace boost::iostreams;
        std::vector<std::string> v = vm["posix-echo-two"].as<std::vector<std::string> >();
        int fd1 = boost::lexical_cast<int>(v[0]);
        file_descriptor_sink sink1(fd1, close_handle);
        stream<file_descriptor_sink> os1(sink1);
        os1 << v[1] << std::endl;
        int fd2 = boost::lexical_cast<int>(v[2]);
        file_descriptor_sink sink2(fd2, close_handle);
        stream<file_descriptor_sink> os2(sink2);
        os2 << v[3] << std::endl;
    }
#elif defined(BOOST_WINDOWS_API)
    else if (vm["windows-print-showwindow"].as<bool>())
    {
        STARTUPINFO si;
        GetStartupInfo(&si);
        std::cout << si.wShowWindow << std::endl;
    }
    else if (vm["windows-print-flags"].as<bool>())
    {
        STARTUPINFO si;
        GetStartupInfo(&si);
        std::cout << si.dwFlags << std::endl;
    }
#endif
    return EXIT_SUCCESS;
}
