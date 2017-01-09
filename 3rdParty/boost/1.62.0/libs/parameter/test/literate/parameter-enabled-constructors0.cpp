
#include <boost/parameter.hpp>
#include <iostream>
BOOST_PARAMETER_NAME(name)
BOOST_PARAMETER_NAME(index)

struct myclass_impl
{
    template <class ArgumentPack>
    myclass_impl(ArgumentPack const& args)
    {
        std::cout << "name = " << args[_name]
                  << "; index = " << args[_index | 42]
                  << std::endl;
    }
};

struct myclass : myclass_impl
{
    BOOST_PARAMETER_CONSTRUCTOR(
        myclass, (myclass_impl), tag
      , (required (name,*)) (optional (index,*))) // no semicolon
};


int main() {
myclass x("bob", 3);                     // positional
myclass y(_index = 12, _name = "sally"); // named
myclass z("june");                       // positional/defaulted
}

