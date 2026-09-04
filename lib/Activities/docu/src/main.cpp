#include "find_activity_subclasses.h"
#include "markdown.h"
#include "options.h"
#include "repository.h"
#include "sources.h"
#include "logging.h"

#include <string_view>
#include <variant>
#include <vector>

int main(int argc, char const** argv) {
  // 1. parse command line options
  auto const parsed =
      options::parse(std::vector<std::string_view>(argv + 1, argv + argc));
  if (std::holds_alternative<options::HelpRequested>(parsed)) {
    std::cout << options::usage();
    return 0;
  }
  if (auto const* error = std::get_if<options::Error>(&parsed)) {
    log::err("{}\n\n{}", error->message, options::usage());
    return 1;
  }
  auto const& [build_dir, source_paths] = std::get<options::Options>(parsed);

  // 2. verify input; get_database reports a fatal error here, get_sources warns
  // about individual skipped paths itself
  auto const database = sources::get_database(build_dir);
  if (auto const* error = std::get_if<sources::Error>(&database)) {
    log::err(error->message);
    return 1;
  }
  auto const& compilation_database = *std::get<sources::Database>(database);

  auto const source_files =
      sources::get_sources(compilation_database, source_paths);
  if (source_files.empty()) {
    log::err("no source file to scan");
    return 1;
  }

  // 3. execute search and output-formatting
  std::cout << activities_to_markdown(
      find_all_activities(compilation_database, source_files),
      repository::commit_ids(compilation_database, source_paths));

  return 0;
}
