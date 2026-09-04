#pragma once

#include "activity_declaration.h"

#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <vector>

/**
 * Find every Activity-subclass declaration owned somewhere in `sources`.
 *
 * `sources` are translation units of `database` (see sources.h for how they are
 * selected from the user's paths). For each one, ClangTool is run to collect
 * every concrete Activity subclass declared as a member field or a local
 * variable.
 *
 * Example:
 *   auto const database = sources::get_database("build");
 *   for (auto const& activity : find_all_activities(*database,
 * {"arangod/Cluster"})) { std::cout << activity.type << " @ " << activity.owner
 * << "\n";
 *   }
 *
 * This function works the following:
 * - user-defined matchers define which code we are interested in (defined in
 * matcher.h)
 * - for each match, it executes a user-defined callback that converts the match
 * into an ActivityDeclaration and adds it to the output vector (defined in
 * conversion.h)
 */
auto find_all_activities(clang::tooling::CompilationDatabase const& database,
                         std::vector<std::string> const& sources)
    -> std::vector<ActivityDeclaration>;
