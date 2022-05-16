#include "Containers/SmallVector.h"
#include <absl/container/inlined_vector.h>

static_assert(sizeof(absl::InlinedVector<char, 1>) == 3 * sizeof(std::size_t));
static_assert(sizeof(arangodb::containers::SmallVector<char, 1>) >
              3 * sizeof(std::size_t));
