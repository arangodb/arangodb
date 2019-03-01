
#include <boost/parameter.hpp>
#include <iostream>
namespace lib
{
  BOOST_PARAMETER_NAME(name)
  BOOST_PARAMETER_NAME(index)

  BOOST_PARAMETER_FUNCTION(
    (int), f, tag,
    (optional (name,*,"bob")(index,(int),1))
  )
  {
      std::cout << name << ":" << index << std::endl;
      return index;
  }
}
int x = lib::f(lib::_name = "jill", lib::_index = 1);
int main() {}

