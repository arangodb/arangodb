#pragma once

#include "activity_declaration.h"

#include "clang/ASTMatchers/ASTMatchFinder.h"
#include <unordered_set>

namespace conversion {

/**
 * MatchFinder callback: collects one ActivityDeclaration per match into the
 * caller-supplied output vector.
 *
 * Dedupes by `owner_file:owner_line`, so the same physical declaration isn't
 * reported across the many TUs that include the same header — but distinct
 * declarations of the same Activity class (e.g. a member field plus a
 * `make<T>` call site elsewhere) both survive. Location-based filtering
 * (system headers, 3rdParty/, build outputs, Activity library internals) is
 * applied in the matcher itself.
 */
class ActivityCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  explicit ActivityCallback(std::vector<ActivityDeclaration>& out)
      : _out_activities(out) {}

  auto run(clang::ast_matchers::MatchFinder::MatchResult const& result)
      -> void override;

 private:
  std::vector<ActivityDeclaration>& _out_activities;
  std::unordered_set<std::string> _seen_activities;
};

}  // namespace conversion
