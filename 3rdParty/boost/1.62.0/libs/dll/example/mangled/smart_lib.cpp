// Copyright 2016 Klemens D. Morgenstern.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp" // contains dll_test::replace_with_full_path

//[smart_lib_setup
#include <boost/dll/smart_library.hpp> // for import_alias
#include <iostream>
#include <memory>

namespace dll = boost::dll;

struct alias;

int main(int argc, char* argv[]) {
    /*<-*/ b2_workarounds::argv_to_path_guard guard(argc, argv); /*->*/
    boost::filesystem::path lib_path(argv[1]);   // argv[1] contains path to directory with our plugin library
    dll::smart_lib lib(lib_path);                // smart library instance
//]
//[smart_lib_size    
    auto size_f = lib.get_function<std::size_t()>("space::my_plugin::size"); //get the size function
    
    auto size = size_f();             // get the size of the class
    std::unique_ptr<char[], size> buffer(new char[size]);    //allocate a buffer for the import
    alias & inst = *reinterpret_cast<alias*>(buffer.get()); //cast it to our alias type.
//]
//[smart_lib_type_alias    
    lib.add_type_alias("space::my_plugin"); //add an alias, so i can import a class that is not declared here
//]
//[smart_lib_ctor    
    auto ctor = lib.get_constructor<alias(const std::string&)>(); //get the constructor
    ctor.call_standard(&inst, "MyName"); //call the non-allocating constructor. The allocating-constructor is a non-portable feature
//]
//[smart_lib_name
    auto name_f = lib.get_mem_fn<const alias, std::string()>("name");//import the name function
    std::cout <<  "Name Call: " << (inst.*name_f)() << std::endl;
//]
//[smart_lib_calculate
    //import both calculate functions
    auto calc_f = lib.get_mem_fn<alias, float(float, float)>("calculate");
    auto calc_i = lib.get_mem_fn<alias, int(int, int)>      ("calculate");
    
    std::cout << "calc(float): " << (inst.*calc_f)(5., 2.) << std::endl;
    std::cout << "calc(int)  : " << (inst.*calc_f)(5,   2) << std::endl;
//]
//[smart_lib_var
    auto & var = lib.get_variable<int>("space::my_plugin::value");
    cout << "value " << var << endl;
//]
//[smart_lib_dtor    
    auto dtor = lib.get_destructor<alias>(); //get the destructor
    dtor.call_standard(&inst);
    std::cout << "plugin->calculate(1.5, 1.5) call:  " << plugin->calculate(1.5, 1.5) << std::endl;
}
//]
