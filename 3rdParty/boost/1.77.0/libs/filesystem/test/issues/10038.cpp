#include <boost/filesystem.hpp>

int main(void)
{
    boost::filesystem::copy_file("a", "b");
    return 0;
}
