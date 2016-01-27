#include "Log.h"

using namespace arangodb::consensus;

Log::Log() {}
Log::~Log() {}

Slice const& Log::log (Slice const& slice) {
    return slice;
}
