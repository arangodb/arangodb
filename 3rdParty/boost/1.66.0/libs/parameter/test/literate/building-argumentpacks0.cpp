
#include <boost/parameter.hpp>
#include <iostream>
BOOST_PARAMETER_NAME(index)

template <class ArgumentPack>
int print_index(ArgumentPack const& args)
{
    std::cout << "index = " << args[_index] << std::endl;
    return 0;
}

int x = print_index(_index = 3);  // prints "index = 3"

BOOST_PARAMETER_NAME(name)

template <class ArgumentPack>
int print_name_and_index(ArgumentPack const& args)
{
    std::cout << "name = " << args[_name] << "; ";
    return print_index(args);
}

int y = print_name_and_index((_index = 3, _name = "jones"));


namespace parameter = boost::parameter;
using parameter::required;
using parameter::optional;
using boost::is_convertible;
using boost::mpl::_;
parameter::parameters<
    required<tag::name, is_convertible<_,char const*> >
  , optional<tag::index, is_convertible<_,int> >
> spec;

char const sam[] = "sam";
int twelve = 12;

int z0 = print_name_and_index( spec(sam, twelve) );

int z1 = print_name_and_index(
   spec(_index=12, _name="sam")
);
int main()
{}

