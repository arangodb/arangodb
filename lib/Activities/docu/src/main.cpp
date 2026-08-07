#include "find_activity_subclasses.h"
#include "markdown.h"

#include <iostream>

int main(int argc, char const** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <path>\n"
              << "  path is a source file or a directory; "
                 "compile_commands.json is auto-detected.\n";
    return 1;
  }

  std::cout << activities_to_markdown(find_all_activities(argv[1]));
  return 0;
}
