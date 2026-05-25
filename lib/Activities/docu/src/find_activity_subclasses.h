#pragma once

#include "activity_declaration.h"
#include <string>
#include <vector>

/**
 * Find every Activity-subclass declaration owned somewhere under `path`.
 *
 * `path` is a single source file or a directory; compile_commands.json is
 * auto-detected. Headers are routed to their sibling .cpp so ClangTool has
 * a translation unit to parse.
 *
 * Example:
 *   auto activities = find_all_activities(
 *       "arangod/StorageEngine/TransactionState.cpp");
 *   for (auto const& activity : activities) {
 *     std::cout << activity.data_type << " @ " << activity.owner_file << "\n";
 *   }
 *
 * This functions works the following:
 * - runs over all the sources (module sources) we specify
 * - user-defined matchers define which code we are interested in (module
 * matcher)
 * - for each match, it executes a user-defined callback that converts the match
 * into an ActivityDeclaration and adds it to the output vector (module
 * conversion)
 */
auto find_all_activities(std::string const& path)
    -> std::vector<ActivityDeclaration>;
