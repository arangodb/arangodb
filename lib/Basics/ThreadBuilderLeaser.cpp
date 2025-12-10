#include "Basics/ThreadBuilderLeaser.h"

namespace arangodb {
thread_local ThreadBuilderLeaser ThreadBuilderLeaser::current;
}
