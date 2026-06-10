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
 * Where it lives, the spelled Data type, and the shape of that Data (one level
 * of nested records).
 *
 * Example:
 *   ActivityDeclaration{
 *     .owner_file = "arangod/StorageEngine/TransactionState.h",
 *     .owner_line = 521,
 *     .data_type  = "arangodb::transaction::activity::TransactionActivityData",
 *     .field_types = {
 *       Struct{.name = "TransactionActivityData",
 *              .fields = {Member{.name = "user", .type = "std::string"}}}}};
 *
 * The Snapshot envelope (id, parent, type, created) is the same for every
 * Activity and is intentionally not modeled here. `field_types` is empty
 * when the Data is a dynamic container (e.g. std::unordered_map) or when
 * the Data type couldn't be resolved.
 */
struct ActivityDeclaration {
  std::string owner_file;
  unsigned owner_line = 0;
  std::string type;
  std::optional<std::string> data_type;
  std::vector<Struct> type_definition;

  auto operator==(ActivityDeclaration const&) const -> bool = default;
};
