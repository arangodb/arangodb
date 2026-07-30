#include "ExportFeatureOptions.h"

#include "Basics/error.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"

#include <filesystem>

namespace arangodb {
ExportFeatureOptions::ExportFeatureOptions() {
  using basics::FileUtils::buildFilename;
  std::error_code ec;
  std::filesystem::path const cwd = std::filesystem::current_path(ec);
  if (ec) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_set_errno(TRI_ERROR_SYS_ERROR),
        basics::StringUtils::concatT("cannot get current working directory: ",
                                     ec.message()));
  }
  outputDirectory = buildFilename(cwd.string(), "export");
}
}  // namespace arangodb
