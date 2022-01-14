// Before running this test: export LANG=foo

#include <boost/filesystem.hpp>
int main()
{
    boost::filesystem::path("/abc").root_directory();
}
