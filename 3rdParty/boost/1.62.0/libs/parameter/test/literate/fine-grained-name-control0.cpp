
#include <boost/parameter.hpp>
BOOST_PARAMETER_NAME((pass_foo, keywords) foo)

BOOST_PARAMETER_FUNCTION(
  (int), f,
  keywords, (required (foo, *)))
{
    return foo + 1;
}

int x = f(pass_foo = 41);
int main()
{}

