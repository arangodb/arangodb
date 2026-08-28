#pragma once

#include "activity_declaration.h"
#include <string>
#include <string_view>
#include <vector>

/**
 * Render found activities as a single markdown document.
 *
 * `commit_id` names the checkout the activities were read from; pass a
 * placeholder such as "unknown" when it cannot be determined.
 *
 * # Activities
 * This document lists all existing activities in arangodb on commit <commit_id>
 * ...
 *
 * ## <owner>
 * type: <activity type>

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
 *   auto const md = activities_to_markdown(find_all_activities("foo.cpp"),
 *                                          "f424edc2d14");
 *   std::cout << md;
 */
auto activities_to_markdown(std::vector<ActivityDeclaration> const& activities,
                            std::string_view commit_id) -> std::string;
