#include <boost/filesystem.hpp>
#include <boost/range.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptors.hpp>
#include <vector>
#include <iostream>

namespace fs = boost::filesystem;
using namespace boost::adaptors;

int main()
{
    fs::recursive_directory_iterator beg("."), end;

    auto fileFilter = [](fs::path const& path) {
        return is_regular_file(path);
    };

    std::vector< fs::path > paths;
    copy(boost::make_iterator_range(beg, end) | filtered(fileFilter), std::back_inserter(paths));

    for (auto& p : paths)
        std::cout << p << "\n";
}