#pragma once

#include <optional>
#include <string>
#include <vector>

/**
 * A single public field of a record: its name and source-spelled type.
 *
 * Example:
 *   Member{.name = "user", .type = "std::string"};
 */
struct Member {
  std::string name;
  std::string type;

  auto operator==(Member const&) const -> bool = default;
};

/**
 * A named record with the list of its public fields.
 *
 * Used both for an activity's Data record itself and for project-local
 * records reached one level deep from any of its members.
 *
 * Example:
 *   Struct{.name = "TransactionCollection",
 *          .fields = {Member{.name = "name", .type = "std::string"}}};
 */
struct Struct {
  std::string name;
  std::vector<Member> fields;

  auto operator==(Struct const&) const -> bool = default;
};

/**
 * Describes one Activity subclass declaration
 *
 * Names the owner (the record or function that declares the activity), the
 * fully-qualified Activity subclass, and the shape of its Data: the Data record
 * followed by the project-local records reached recursively from its fields.
 *
 * Example:
 *   ActivityDeclaration{
 *     .owner = "arangodb::transaction::TransactionState",
 *     .type  = "arangodb::transaction::activity::TransactionActivity",
 *     .data_type_definition = {
 *       Struct{.name =
 * "arangodb::transaction::activity::TransactionActivityData", .fields =
 * {Member{.name = "user", .type = "std::string"}}}}};
 *
 * The Activity envelope (id, parent, type, created) is the same for every
 * Activity and is intentionally not modeled here. A Data type that is a std
 * container alias (e.g. std::unordered_map) appears as a single fieldless
 * Struct named after the alias.
 */
struct ActivityDeclaration {
  std::string owner;
  std::string type;
  std::vector<Struct> data_type_definition;

  auto operator==(ActivityDeclaration const&) const -> bool = default;
};
