#include <boost/container_hash/hash.hpp>
#include <vector>
#include <algorithm>
#include <iterator>
#include <cassert>

//[ get_hashes
template <class Container>
std::vector<std::size_t> get_hashes(Container const& x)
{
    std::vector<std::size_t> hashes;
    std::transform(x.begin(), x.end(), std::back_inserter(hashes),
        boost::hash<typename Container::value_type>());

    return hashes;
}
//]

int main() {
    std::vector<int> values;
    values.push_back(10);
    values.push_back(20);

    std::vector<std::size_t> hashes = get_hashes(values);
    assert(hashes[0] = boost::hash<int>()(values[0]));
    assert(hashes[1] = boost::hash<int>()(values[1]));
}
