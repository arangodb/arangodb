// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/dll.hpp>
#include <boost/function.hpp>
#include <string>
#include "b2_workarounds.hpp"

// Unit Tests
int main(int argc, char* argv[]) {
    using namespace boost;

    boost::dll::fs::path path_to_shared_library = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(path_to_shared_library.string().find("getting_started_library") != std::string::npos);
    
    //[getting_started_imports_c_function
    // Importing pure C function
    function<int(int)> c_func = dll::import_symbol<int(int)>(
            path_to_shared_library, "c_func_name"
        );
    //]

    int c_func_res = c_func(1); // calling the function
    BOOST_TEST(c_func_res == 2);


    //[getting_started_imports_c_variable
    // Importing pure C variable
    shared_ptr<int> c_var = dll::import_symbol<int>(
            path_to_shared_library, "c_variable_name"
        );
    //]

    int c_var_old_contents = *c_var; // using the variable
    *c_var = 100;
    BOOST_TEST(c_var_old_contents == 1);


    //[getting_started_imports_alias
    // Importing function by alias name
   /*<-*/ function<std::string(const std::string&)> /*->*/ /*=auto*/ cpp_func = dll::import_alias<std::string(const std::string&)>(
            path_to_shared_library, "pretty_name"
        );
    //]

    // calling the function
    std::string cpp_func_res = cpp_func(std::string("In importer.")); 
    BOOST_TEST(cpp_func_res == "In importer. Hello from lib!");

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES) && !defined(BOOST_NO_CXX11_AUTO_DECLARATIONS)
    //[getting_started_imports_cpp11_function
    // Importing function.
    auto cpp11_func = dll::import_symbol<int(std::string&&)>(
            path_to_shared_library, "i_am_a_cpp11_function"
        );
    //]

    // calling the function
    int cpp11_func_res = cpp11_func(std::string("In importer.")); 
    BOOST_TEST(cpp11_func_res == sizeof("In importer.") - 1);
#endif


    //[getting_started_imports_cpp_variable
    // Importing  variable.
    shared_ptr<std::string> cpp_var = dll::import_symbol<std::string>(
            path_to_shared_library, "cpp_variable_name"
        );
    //]

    std::string cpp_var_old_contents = *cpp_var; // using the variable
    *cpp_var = "New value";
    BOOST_TEST(cpp_var_old_contents == "some value");
    BOOST_TEST(*cpp_var == "New value");

    return boost::report_errors();
}

