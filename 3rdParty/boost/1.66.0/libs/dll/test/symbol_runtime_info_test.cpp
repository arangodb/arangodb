// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2017 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../example/b2_workarounds.hpp"
#include <boost/dll.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/filesystem/operations.hpp>

#include <boost/core/lightweight_test.hpp>

#include <exception> // std::set_terminate
#include <signal.h> // ::signal

// lib functions

typedef float (lib_version_func)();
typedef void  (say_hello_func)  ();
typedef int   (increment)       (int);

// exe function
extern "C" int BOOST_SYMBOL_EXPORT exef() {
    return 15;
}


extern "C" void BOOST_SYMBOL_EXPORT my_terminate_handler() {
    std::abort();
}

extern "C" void BOOST_SYMBOL_EXPORT my_signal_handler(int) {
    std::abort();
}

void internal_function() {}
int internal_variable = 1;

// Unit Tests
int main(int argc, char* argv[]) {
    using namespace boost::dll;

    boost::filesystem::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("test_library") != std::string::npos);

    shared_library lib(shared_library_path);

    std::cout << std::endl;
    std::cout << "shared_library: " << shared_library_path << std::endl;
    std::cout << "symbol_location: " << symbol_location(lib.get<int>("integer_g")) << std::endl;
    std::cout << "lib.location():      " << lib.location() << std::endl;
    BOOST_TEST(
        symbol_location(lib.get<int>("integer_g")) == lib.location()
    );

    BOOST_TEST(
        symbol_location(lib.get<say_hello_func>("say_hello")) == lib.location()
    );

    BOOST_TEST(
        symbol_location(lib.get<lib_version_func>("lib_version")) == lib.location()
    );

    BOOST_TEST(
        symbol_location(lib.get<const int>("const_integer_g")) == lib.location()
    );

    // Checking that symbols are still available, after another load+unload of the library
    { shared_library sl2(shared_library_path); }

    BOOST_TEST(
        symbol_location(lib.get<int>("integer_g")) == lib.location()
    );

    // Checking aliases
    BOOST_TEST(
        symbol_location(lib.get<std::size_t(*)(const std::vector<int>&)>("foo_bar")) == lib.location()
    );
    BOOST_TEST(
        symbol_location(lib.get_alias<std::size_t(const std::vector<int>&)>("foo_bar")) == lib.location()
    );


    BOOST_TEST(
        symbol_location(lib.get<std::size_t*>("foo_variable")) == lib.location()
    );
    BOOST_TEST(
        symbol_location(lib.get_alias<std::size_t>("foo_variable")) == lib.location()
    );
    
    { // self
        shared_library sl(program_location());
        BOOST_TEST(
            (boost::filesystem::equivalent(symbol_location(sl.get<int(void)>("exef")), argv[0]))
        );
    }

    { // self with error_code
        boost::system::error_code ec;
        shared_library sl(program_location(ec));
        BOOST_TEST(!ec);

        BOOST_TEST(
            (boost::filesystem::equivalent(symbol_location(sl.get<int(void)>("exef"), ec), argv[0]))
        );
        BOOST_TEST(!ec);

        symbol_location(&sl.get<int(void)>("exef"), ec);
        BOOST_TEST(ec);
    }

    std::cout << "\ninternal_function: " << symbol_location(internal_function);
    std::cout << "\nargv[0]          : " << boost::filesystem::absolute(argv[0]);
    BOOST_TEST(
        (boost::filesystem::equivalent(symbol_location(internal_function), argv[0]))
    );

    BOOST_TEST(
        (boost::filesystem::equivalent(symbol_location(internal_variable), argv[0]))
    );


    BOOST_TEST(
        (boost::filesystem::equivalent(this_line_location(), argv[0]))
    );

    { // this_line_location with error_code
        boost::system::error_code ec;
        BOOST_TEST(
            (boost::filesystem::equivalent(this_line_location(ec), argv[0]))
        );
        BOOST_TEST(!ec);
    }

    BOOST_TEST(
        lib.get_alias<boost::filesystem::path()>("module_location_from_itself")() == lib.location()
    );

    // Checking docs content
    std::cout << "\nsymbol_location(std::cerr); // " << symbol_location(std::cerr);
    std::cout << "\nsymbol_location(std::puts); // " << symbol_location(std::puts);

    std::set_terminate(&my_terminate_handler);
    BOOST_TEST((boost::filesystem::equivalent(
        symbol_location_ptr(std::set_terminate(0)),
        argv[0]
    )));

    {
        boost::system::error_code ec;
        boost::filesystem::path p = symbol_location_ptr(std::set_terminate(0), ec);
        BOOST_TEST(ec || !p.empty());
    }

    {
        boost::system::error_code ec;
        symbol_location(std::set_terminate(0), ec),
        BOOST_TEST(ec);
    }

    {
        std::set_terminate(&my_terminate_handler);
        boost::system::error_code ec;
        symbol_location(std::set_terminate(0), ec),
        BOOST_TEST(ec);
    }

    {
        boost::system::error_code ec;
        ::signal(SIGSEGV, &my_signal_handler);
        boost::filesystem::path p = symbol_location_ptr(::signal(SIGSEGV, SIG_DFL), ec);
        BOOST_TEST((boost::filesystem::equivalent(
            p,
            argv[0]
        )) || ec);
    }

    {
        ::signal(SIGSEGV, &my_signal_handler);
        boost::system::error_code ec;
        symbol_location(::signal(SIGSEGV, SIG_DFL), ec);
        BOOST_TEST(ec);
    }


    return boost::report_errors();
}

