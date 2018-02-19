
#include <boost/parameter.hpp>
#include <cassert>
BOOST_PARAMETER_NAME(name)
BOOST_PARAMETER_NAME(index)

template <class Name, class Index>
int deduce_arg_types_impl(Name& name, Index& index)
{
    Name& n2 = name;  // we know the types
    Index& i2 = index;
    return index;
}

template <class ArgumentPack>
int deduce_arg_types(ArgumentPack const& args)
{
    return deduce_arg_types_impl(args[_name], args[_index|42]);
}
int a1 = deduce_arg_types((_name = "foo"));
int a2 = deduce_arg_types((_name = "foo", _index = 3));

int main()
{
    assert(a1 == 42);
    assert(a2 == 3);
}

