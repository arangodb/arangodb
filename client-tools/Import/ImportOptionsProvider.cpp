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

#include "Import/ImportOptionsProvider.h"

#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Import/ImportHelper.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void ImportOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, ImportFeatureOptions& opts) {
  options->addOption("--file", "The file to import (\"-\" for stdin).",
                     new StringParameter(&opts.filename));

  options->addOption("--auto-rate-limit",
                     "Adjust the data loading rate automatically, starting at "
                     "`--batch-size` bytes per thread per second.",
                     new BooleanParameter(&opts.autoChunkSize));

  options->addOption("--backslash-escape",
                     "Use backslash as the escape character for quotes. Used "
                     "for CSV imports only.",
                     new BooleanParameter(&opts.useBackslash));

  options->addOption("--batch-size",
                     "The size for individual data batches (in bytes).",
                     new UInt64Parameter(&opts.chunkSize));

  options->addOption(
      "--threads", "Number of parallel import threads.",
      new UInt32Parameter(&opts.threadCount),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--collection",
                     "The name of the collection to import into.",
                     new StringParameter(&opts.collectionName));

  options->addOption(
      "--from-collection-prefix",
      "The collection name prefix to prepend to all values in the "
      "`_from` attribute that only specify a document key.",
      new StringParameter(&opts.fromCollectionPrefix));

  options->addOption(
      "--to-collection-prefix",
      "The collection name prefix to prepend to all values in the "
      "`_to` attribute that only specify a document key.",
      new StringParameter(&opts.toCollectionPrefix));

  options->addOption(
      "--overwrite-collection-prefix",
      "Force the `--from-collection-prefix` and `--to-collection-prefix`, "
      "possibly replacing existing collection name prefixes.",
      new BooleanParameter(&opts.overwriteCollectionPrefix));

  options->addOption(
      "--create-collection",
      "Create the target collection if it does not already exist.",
      new BooleanParameter(&opts.createCollection));

  options->addOption("--create-database",
                     "Create the target database if it does not exist.",
                     new BooleanParameter(&opts.createDatabase));

  options
      ->addOption(
          "--headers-file",
          "The file to read the CSV or TSV column headers from. "
          "If specified, no header is expected in the regular input file.",
          new StringParameter(&opts.headersFile))
      .setIntroducedIn(30800);

  options->addOption(
      "--skip-lines",
      "The number of lines to skip of the input file (CSV and TSV only).",
      new UInt64Parameter(&opts.rowsToSkip));

  options
      ->addOption(
          "--max-errors",
          "The maximum number of errors after which the import will stop.",
          new UInt64Parameter(&opts.maxErrors))
      .setIntroducedIn(31200)
      .setLongDescription(R"(This is not an exact limit for the number of
errors. arangoimport sends data to the server in batches, and likely also in
parallel. The server processes these in-flight batches regardless of the maximum
number of errors configured here. However, arangoimport stops processing more
input data once the server reported at least this many errors back.)");

  options->addOption(
      "--convert",
      "Convert the strings `null`, `false`, `true` and strings "
      "containing numbers into non-string types. For CSV and TSV "
      "only.",
      new BooleanParameter(&opts.convert));

  options->addOption(
      "--translate",
      "Define a mapping for a column header to an attribute name "
      "using the syntax \"from=to\". You can specify this "
      "startup option multiple times. For CSV and TSV only.",
      new VectorParameter<StringParameter>(&opts.translations));

  options
      ->addOption(
          "--datatype",
          "Force a specific datatype for an attribute "
          "(null/boolean/number/string) using the syntax \"attribute=type\". "
          "For CSV and TSV only. Takes precedence over `--convert`.",
          new VectorParameter<StringParameter>(&opts.datatypes))
      .setIntroducedIn(30900);

  options->addOption(
      "--remove-attribute",
      "remove an attribute before inserting documents"
      " into collection (for CSV, TSV and JSON only)",
      new VectorParameter<StringParameter>(&opts.removeAttributes));

  std::unordered_set<std::string> types = {"document", "edge"};
  std::vector<std::string> typesVector(types.begin(), types.end());
  std::string typesJoined = basics::StringUtils::join(typesVector, " or ");

  options->addOption("--create-collection-type",
                     "The type of the collection if it needs to be created (" +
                         typesJoined + ").",
                     new DiscreteValuesParameter<StringParameter>(
                         &opts.createCollectionType, types));

  std::unordered_set<std::string> imports = {"csv", "tsv", "json", "jsonl",
                                             "auto"};

  options->addOption(
      "--type", "The format of import file.",
      new DiscreteValuesParameter<StringParameter>(&opts.typeImport, imports));

  options->addOption(
      "--overwrite",
      "Overwrite the collection if it exists. WARNING: This removes any data "
      "from the collection!",
      new BooleanParameter(&opts.overwrite));

  options->addOption(
      "--quote",
      "The character that encloses field values. Used for CSV imports only.",
      new StringParameter(&opts.quote));

  options->addOption(
      "--separator",
      "The field separator. Used for CSV and TSV imports. "
      "Defaults to a comma (CSV) or a tabulation character (TSV).",
      new StringParameter(&opts.separator),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--progress", "Show the progress.",
                     new BooleanParameter(&opts.progress));

  options->addOption("--ignore-missing",
                     "Ignore missing columns in CSV and TSV input.",
                     new BooleanParameter(&opts.ignoreMissing));

  std::unordered_set<std::string> actions = {"error", "update", "replace",
                                             "ignore"};
  std::vector<std::string> actionsVector(actions.begin(), actions.end());
  std::string actionsJoined = basics::StringUtils::join(actionsVector, ", ");

  options->addOption("--on-duplicate",
                     "The action to perform when a unique key constraint "
                     "violation occurs. Possible values: " +
                         actionsJoined,
                     new DiscreteValuesParameter<StringParameter>(
                         &opts.onDuplicateAction, actions));

  options
      ->addOption("--merge-attributes",
                  "Concatenate attributes into a new document attribute, like "
                  "\"mergedAttribute=[someAttribute]-[otherAttribute]\" "
                  "(CSV and TSV only).",
                  new VectorParameter<StringParameter>(&opts.mergeAttributes))
      .setIntroducedIn(30901);

  options->addOption(
      "--latency",
      "Show 10 second latency statistics (values in microseconds).",
      new BooleanParameter(&opts.latencyStats));

  options->addOption("--skip-validation",
                     "Skip document schema validation during import.",
                     new BooleanParameter(&opts.skipValidation));
}

void ImportOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> options, ImportFeatureOptions& opts) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if ((1 == n) && (!options->processingResult().touched("--file"))) {
    // only take positional file name attribute into account if user
    // did not specify the --file option as well
    opts.filename = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("0dc12", FATAL, arangodb::Logger::FIXME)
        << "expecting at most one filename, got " +
               basics::StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  } else if (n > 0) {
    LOG_TOPIC("0dc13", FATAL, arangodb::Logger::FIXME)
        << "Unused commandline arguments: " << positionals;
    FATAL_ERROR_EXIT();
  }

  if (opts.chunkSize > arangodb::import::ImportHelper::kMaxBatchSize) {
    // it's not sensible to raise the batch size beyond this value
    // because the server has a built-in limit for the batch size too
    // and will reject bigger HTTP request bodies
    LOG_TOPIC("e6d71", WARN, arangodb::Logger::FIXME)
        << "capping --batch-size value to "
        << arangodb::import::ImportHelper::kMaxBatchSize;
    opts.chunkSize = arangodb::import::ImportHelper::kMaxBatchSize;
  }

  if (opts.threadCount < 1) {
    // it's not sensible to use just one thread
    LOG_TOPIC("9e3f9", WARN, arangodb::Logger::FIXME)
        << "capping --threads value to " << 1;
    opts.threadCount = 1;
  }
  if (opts.threadCount > NumberOfCores::getValue() * 2) {
    // it's not sensible to use just one thread ...
    //  and import's CPU usage is negligible, real limit is cluster cores
    LOG_TOPIC("aca46", WARN, arangodb::Logger::FIXME)
        << "capping --threads value to " << NumberOfCores::getValue() * 2;
    opts.threadCount = static_cast<uint32_t>(NumberOfCores::getValue()) * 2;
  }

  for (auto const& it : opts.translations) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() < 2) {
      parts.push_back("");
    }
    basics::StringUtils::trimInPlace(parts[0]);
    basics::StringUtils::trimInPlace(parts[1]);

    if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) {
      LOG_TOPIC("83ae7", FATAL, arangodb::Logger::FIXME)
          << "invalid translation '" << it << "'";
      FATAL_ERROR_EXIT();
    }
  }
  for (auto const& it : opts.datatypes) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() < 2) {
      parts.push_back("");
    }
    basics::StringUtils::trimInPlace(parts[0]);
    basics::StringUtils::trimInPlace(parts[1]);

    if (parts.size() != 2 || parts[0].empty() ||
        (parts[1] != "boolean" && parts[1] != "number" && parts[1] != "null" &&
         parts[1] != "string")) {
      LOG_TOPIC("13e75", FATAL, arangodb::Logger::FIXME)
          << "invalid datatype '" << it << "'. valid types are: "
          << "boolean, number, null, string";
      FATAL_ERROR_EXIT();
    }
  }
  for (std::string& str : opts.removeAttributes) {
    basics::StringUtils::trimInPlace(str);
    if (str.empty()) {
      LOG_TOPIC("74cfc", FATAL, arangodb::Logger::FIXME)
          << "cannot remove an empty attribute";
      FATAL_ERROR_EXIT();
    }
  }
}

}  // namespace arangodb
