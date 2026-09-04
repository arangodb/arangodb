#pragma once

#include "activity_declaration.h"
#include "repository.h"

#include <string>
#include <vector>

/**
 * Render found activities as a single markdown document.
 *
 * `commits` names the checkouts the activities were read from; the first is
 * arangodb, an optional second is the enterprise submodule. Each is rendered as
 * "<repository> on commit <id>"; pass "unknown" as an id when it cannot be
 * determined.
 *
 * # Activities
 * This document lists all existing activities in arangodb on commit <id>[,
 * enterprise on commit <id>]
 * ...
 *
 * ## <owner>
 * type: <activity type>
 *
 * ### <type_definition[0].name>
 * | Field                               | Type                                |
 * |-------------------------------------|-------------------------------------|
 * | <type_definition[0].fields[0].name> | <type_definition[0].fields[0].type> |
 * | <type_definition[0].fields[1].name> | <type_definition[0].fields[1].type> |
 * ...
 *
 * ### <type_definition[1].name>
 * | Field                               | Type                                |
 * |-------------------------------------|-------------------------------------|
 * | <type_definition[1].fields[0].name> | <type_definition[1].fields[0].type> |
 * | <type_definition[1].fields[1].name> | <type_definition[1].fields[1].type> |
 * ...
 *
 * Example:
 *   auto const md = activities_to_markdown(find_all_activities(db, files),
 *                                          {Commit{"arangodb", "f424edc"}});
 *   std::cout << md;
 */
auto activities_to_markdown(std::vector<ActivityDeclaration> const& activities,
                            std::vector<repository::Commit> const& commits)
    -> std::string;
