#pragma once

#include "activity_declaration.h"
#include <string>
#include <vector>

/**
 * Render found activities as a single markdown document.
 *
 * # Activities
 *
 * ## <type>
 * owner: <file>:<line>
 * data: <data_type>

 * ### <type_definition[0].name>
 * | Field                               | Type                                |
 * |-------------------------------------+-------------------------------------|
 * | <type_definition[0].fields[0].name> | <type_definition[0].fields[0].type> |
 * | <type_definition[0].fields[1].name> | <type_definition[0].fields[1].type> |
 * ...
 *
 * ### <type_definition[1].name>
 * | Field                               | Type                                |
 * |-------------------------------------+-------------------------------------|
 * | <type_definition[1].fields[0].name> | <type_definition[1].fields[0].type> |
 * | <type_definition[1].fields[1].name> | <type_definition[1].fields[1].type> |
 * ...
 *
 * Example:
 *   auto const md = activities_to_markdown(find_all_activities("foo.cpp"));
 *   std::cout << md;
 */
auto activities_to_markdown(std::vector<ActivityDeclaration> const& activities)
    -> std::string;
