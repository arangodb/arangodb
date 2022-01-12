#include <boost/static_assert.hpp>

int main()
{
    int const x = 5;
    BOOST_STATIC_ASSERT( x > 4 );
}
