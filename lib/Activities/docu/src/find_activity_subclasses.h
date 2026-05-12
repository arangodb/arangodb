#pragma once

#include <string>
#include <vector>

struct Member {
  std::string name;
  std::string type;

  auto operator==(Member const&) const -> bool = default;
};

struct Struct {
  std::string name;
  std::vector<Member> fields;

  auto operator==(Struct const&) const -> bool = default;
};

// Describes the data slice of one Activity subclass. The Snapshot envelope
// (id, parent, type, created) is the same for every Activity and is not
// modeled here.
//
// `owner_file` / `owner_line` point at the variable declaration (member or
// non-parameter local) whose underlying type — after peeling `shared_ptr` /
// `unique_ptr` — is the Activity subclass.
//
// `data_type` is the fully-qualified spelled type of the Data template arg.
//
// `field_types`:
//   - empty: data is either a dynamic container (e.g. std::unordered_map) or
//     the Data type couldn't be resolved from a GuardedActivity base.
//   - non-empty: first entry is the data record itself; subsequent entries
//     are project-local nested records reached through one container layer
//     (e.g. TransactionCollection via std::vector<TransactionCollection>).
struct ActivityDeclaration {
  std::string owner_file;
  unsigned owner_line = 0;
  std::string data_type;
  std::vector<Struct> field_types;

  auto operator==(ActivityDeclaration const&) const -> bool = default;
};

// Walks the AST of every translation unit at or under `path` (a single source
// file or a directory) and returns one ActivityData per discovered Activity
// subclass declaration. `compile_commands.json` is auto-detected from `path`.
auto find_all_activities(std::string const& path)
    -> std::vector<ActivityDeclaration>;
