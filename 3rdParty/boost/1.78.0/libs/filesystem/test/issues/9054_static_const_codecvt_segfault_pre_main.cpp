#include "boost/filesystem.hpp"

static const boost::filesystem::path::codecvt_type& dummy =
    boost::filesystem::path::codecvt();

int main()
{
    return 0;
}
