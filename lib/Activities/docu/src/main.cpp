#include "find_activity_subclasses.h"

#include <iostream>
#include <string>

int main(int argc, char const** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << argv[0] << " <path>\n"
              << "  path is a source file or a directory; "
                 "compile_commands.json is auto-detected\n";
    return 1;
  }

  auto const results = find_all_activities(argv[1]);

  for (auto const& activity : results) {
    std::cout << activity.type << "  (" << activity.owner_file << ":"
              << activity.owner_line << ")  ";
    if (activity.data_type.has_value()) {
      std::cout << activity.data_type.value();
    }
    if (not activity.type_definition.empty()) {
      std::cout << " with " << activity.type_definition.size() << " struct(s)";
    }
    std::cout << "\n";
  }
  return 0;
}
