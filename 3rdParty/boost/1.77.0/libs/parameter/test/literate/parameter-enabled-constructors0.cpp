
#include <boost/parameter.hpp>
#include <iostream>

BOOST_PARAMETER_NAME(name)
BOOST_PARAMETER_NAME(index)

struct myclass_impl
{
    template <typename ArgumentPack>
    myclass_impl(ArgumentPack const& args)
    {
        std::cout << "name = " << args[_name];
        std::cout << "; index = " << args[_index | 42];
        std::cout << std::endl;
    }
};

struct myclass : myclass_impl
{
    BOOST_PARAMETER_CONSTRUCTOR(
        myclass, (myclass_impl), tag
      , (required (name,*)) (optional (index,*))
    ) // no semicolon
};

#include <boost/core/lightweight_test.hpp>

int main()
{
    myclass x("bob", 3);                     // positional
    myclass y(_index = 12, _name = "sally"); // named
    myclass z("june");                       // positional/defaulted
    return boost::report_errors();
}

