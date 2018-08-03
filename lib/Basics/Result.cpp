#include "Result.h"
#include <sstream>

namespace arangodb {
Result prefixResultMessage(Result const& res, std::string const& prefix){
  std::stringstream err;
  err << prefix << ": " << res.errorMessage();
  return Result{res.errorNumber(), err.str()};
}
}
