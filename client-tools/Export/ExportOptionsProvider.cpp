////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Export/ExportOptionsProvider.h"

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

namespace arangodb {

using namespace arangodb::options;

void ExportOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    ExportFeatureOptions& opts) {
  options->addOption("--collection",
                     "Restrict the export to this collection name (can be "
                     "specified multiple times).",
                     new VectorParameter<StringParameter>(&opts.collections));

  options->addOldOption("--query", "custom-query");
  options->addOption(
      "--custom-query",
      "An AQL query to run for computing the data you want to export.",
      new StringParameter(&opts.customQuery));
  options->addOldOption("--query-max-runtime", "custom-query-max-runtime");
  options
      ->addOption(
          "--custom-query-max-runtime",
          "The runtime threshold for AQL queries (in seconds, 0 = no limit).",
          new DoubleParameter(&opts.customQueryMaxRuntime))
      .setIntroducedIn(30800);
  options
      ->addOption("--custom-query-file",
                  "A path to a file with the custom query to be used.",
                  new StringParameter(&opts.customQueryFile))
      .setIntroducedIn(31000);

  options
      ->addOption("--custom-query-bindvars",
                  "The bind parameters to be used with the `--custom-query` or "
                  "`--custom-query-file` option.",
                  new StringParameter(&opts.customQueryBindVars))
      .setIntroducedIn(31000);

  options->addOption("--graph-name", "The name of a graph to export.",
                     new StringParameter(&opts.graphName));

  options->addOption("--xgmml-label-only", "Export XGMML label only.",
                     new BooleanParameter(&opts.xgmmlLabelOnly));

  options->addOption(
      "--xgmml-label-attribute",
      "Specify the document attribute to use as the XGMML label.",
      new StringParameter(&opts.xgmmlLabelAttribute));

  options->addOption("--output-directory", "The output directory.",
                     new StringParameter(&opts.outputDirectory));

  options
      ->addOption("--documents-per-batch",
                  "The number of documents to return in each batch.",
                  new UInt64Parameter(&opts.documentsPerBatch))
      .setIntroducedIn(30800);

  options
      ->addOption("--escape-csv-formulae",
                  "Prefix string cells in CSV output with extra single quote "
                  "to prevent formula injection.",
                  new BooleanParameter(&opts.escapeCsvFormulae))
      .setIntroducedIn(30805);

  options->addOption("--overwrite",
                     "Overwrite the data in the output directory.",
                     new BooleanParameter(&opts.overwrite));

  options->addOption("--progress", "Show the progress.",
                     new BooleanParameter(&opts.progress));

  options->addOption(
      "--fields", "A comma-separated list of fields to export to a CSV file.",
      new StringParameter(&opts.csvFieldOptions));

  std::unordered_set<std::string> exports = {"csv", "json", "jsonl", "xgmml",
                                             "xml"};
  options->addOption(
      "--type", "type of export",
      new DiscreteValuesParameter<StringParameter>(&opts.typeExport, exports));

  options->addOption("--compress-output",
                     "Compress files containing collection contents using the "
                     "gzip format.",
                     new BooleanParameter(&opts.useGzip));
}

void ExportOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options,
    ExportFeatureOptions& opts) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    opts.outputDirectory = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("71137", FATAL, Logger::CONFIG)
        << "expecting at most one directory, got " +
               basics::StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!opts.outputDirectory.empty() &&
      opts.outputDirectory.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(opts.outputDirectory.size() > 0);
    opts.outputDirectory.pop_back();
  }
  TRI_NormalizePath(opts.outputDirectory);

  if (!opts.customQueryFile.empty()) {
    if (!opts.customQuery.empty()) {
      LOG_TOPIC("2b57e", FATAL, Logger::CONFIG)
          << "expecting either `--custom-query` or `--custom-query-file'";
      FATAL_ERROR_EXIT();
    }

    try {
      basics::FileUtils::slurp(opts.customQueryFile, opts.customQuery);
    } catch (std::exception const& ex) {
      LOG_TOPIC("45275", FATAL, Logger::CONFIG)
          << "unable to read custom query from file '" << opts.customQueryFile
          << "': " << ex.what();
      FATAL_ERROR_EXIT();
    }
  }

  if (opts.graphName.empty() && opts.collections.empty() &&
      opts.customQuery.empty()) {
    LOG_TOPIC("488d8", FATAL, Logger::CONFIG)
        << "expecting at least one collection, a graph name or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (!opts.customQuery.empty() &&
      (!opts.collections.empty() || !opts.graphName.empty())) {
    LOG_TOPIC("6ff88", FATAL, Logger::CONFIG)
        << "expecting either a list of collections or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (!opts.customQueryBindVars.empty()) {
    try {
      opts.customQueryBindVarsBuilder =
          VPackParser::fromJson(opts.customQueryBindVars);
    } catch (...) {
      LOG_TOPIC("bafc2", FATAL, arangodb::Logger::CONFIG)
          << "For flag '--custom-query-bindvars " << opts.customQueryBindVars
          << "': invalid JSON format.";
      FATAL_ERROR_EXIT();
    }
  }

  if (opts.typeExport == "xgmml" && opts.graphName.empty()) {
    LOG_TOPIC("2c3be", FATAL, Logger::CONFIG)
        << "expecting a graph name to dump a graph";
    FATAL_ERROR_EXIT();
  }

  if ((opts.typeExport == "json" || opts.typeExport == "jsonl" ||
       opts.typeExport == "csv") &&
      opts.collections.empty() && opts.customQuery.empty()) {
    LOG_TOPIC("cdcf7", FATAL, Logger::CONFIG)
        << "expecting at least one collection or an AQL query";
    FATAL_ERROR_EXIT();
  }

  if (opts.typeExport == "csv") {
    if (opts.csvFieldOptions.empty()) {
      LOG_TOPIC("76fbf", FATAL, Logger::CONFIG)
          << "expecting at least one field definition";
      FATAL_ERROR_EXIT();
    }

    opts.csvFields = basics::StringUtils::split(opts.csvFieldOptions, ',');
  }

  // we will use _maxRuntime only if the option was set by the user
  opts.useMaxRuntime =
      options->processingResult().touched("--custom-query-max-runtime");
}

}  // namespace arangodb
