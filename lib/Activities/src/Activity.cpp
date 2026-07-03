#include "Activities/Activity.h"

auto arangodb::activities::ActivityPtr::snapshot() -> Snapshot {
  return a->snapshot();
}
