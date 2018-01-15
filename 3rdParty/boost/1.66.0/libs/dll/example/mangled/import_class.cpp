// Copyright 2016 Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../b2_workarounds.hpp" // contains dll_test::replace_with_full_path

//[import_class_setup

#include <boost/dll/smart_library.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/dll/import_class.hpp>



int main(int argc, char* argv[]) {
    /*<-*/ b2_workarounds::argv_to_path_guard guard(argc, argv); /*->*/
    boost::filesystem::path lib_path(argv[1]);   // argv[1] contains path to directory with our plugin library
    smart_library lib(lib_path);// smart library instance
//]
//[import_class_size    
    auto size_f = import_mangled<std::size_t()>("space::my_plugin::size"); //get the size function
    
    auto size = (*size_f)();             // get the size of the class
//]
//[import_class_value
    auto value = import_mangled<int>(lib, "space::my_plugin::value");
//]
//[import_class_ctor
    auto cl = import_class<class alias, const std::string&>(lib, "space::my_plugin::some_class", size, "MyName");
//]
//[import_class_name
    auto name = import_mangled<const alias, std::string()>(lib, "name");
    std::cout << "Name: " << (cl->*name)() << std::endl;
//]
//[import_class_calc
    auto calc = import_mangled<alias, float(float, float), int(int, int)>(lib, "calculate");
    std::cout << "Calc(float): " (cl->*calc)(5.f, 2.f) << std::endl;
    std::cout << "Calc(int):   " (cl->*calc)(5, 2)     << std::endl;
//]
//[import_class_typeinfo
    std::type_info &ti = cl.get_type_info();
//]
}
