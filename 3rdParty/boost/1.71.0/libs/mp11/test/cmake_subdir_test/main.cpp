#include <boost/mp11.hpp>
using namespace boost::mp11;

int main()
{
    using L1 = mp_list<int, float, int, float>;
    return mp_size<mp_unique<L1>>::value == 2? 0: 1;
}
