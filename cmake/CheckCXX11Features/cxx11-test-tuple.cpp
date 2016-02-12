#include <tuple>

int main () {
    typedef std::tuple <int, double, long &, const char *> test_tuple;
    long lengthy = 12;
    test_tuple proof (18, 6.5, lengthy, "Ciao!");
    lengthy = std::get<0>(proof); 
    std::get<3>(proof) = " Beautiful!";
    return 0;
}
