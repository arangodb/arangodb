// given an array of values of a particular type
template<typename T, unsigned int N>
constexpr bool check_symmetry(const T (&value)[N]) {
    using namespace boost::safe_numerics;
    // for each pair of values p1, p2 (100)
    for(unsigned int i = 0; i < N; i++)
    for(unsigned int j = 0; j < N; j++)
        assert(value[i][j] == value[j][i]);
    return true;
}
