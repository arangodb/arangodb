// Rob Conde <rob.conde@ai-solutions.com> reports this fails
// to compile for Boost 1.58 with g++ 4.4.7 but is OK with FC++ 2013

#include "boost/filesystem/operations.hpp"

void myFunc()
{
   using namespace boost::filesystem;

   copy_option opt(copy_option::overwrite_if_exists);

   copy_file(path("p1"),path("p2"),copy_option::overwrite_if_exists);
//   copy_file(path("p1"),path("p2"),opt);
}