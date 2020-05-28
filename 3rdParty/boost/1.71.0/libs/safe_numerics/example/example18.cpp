#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_literal.hpp>

using namespace boost::safe_numerics;

int f(int i){
    return i;
}

template<intmax_t N>
using safe_literal = safe_signed_literal<N, native, loose_trap_policy>;

int main(){
    const long x = 97;
    f(x);   // OK - implicit conversion to int
    const safe_literal<97> y;
    f(y);   // OK - y is a type with min/max = 97;
    return 0;
}