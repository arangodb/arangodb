#include "DumpFeatureOptions.h"

#include "Basics/error.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"

#include <filesystem>

namespace arangodb {
DumpFeatureOptions::DumpFeatureOptions() {
  using basics::FileUtils::buildFilename;
  std::error_code ec;
  std::filesystem::path const cwd = std::filesystem::current_path(ec);
  if (ec) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_set_errno(TRI_ERROR_SYS_ERROR),
        basics::StringUtils::concatT("cannot get current working directory: ",
                                     ec.message()));
  }
  outputPath = buildFilename(cwd.string(), "dump");
  threadCount =
      std::max(threadCount, static_cast<uint32_t>(NumberOfCores::getValue()));
}
}  // namespace arangodb
