
//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
#ifndef ROCKSDB_LITE
#include "rocksdb/utilities/ldb_cmd.h"

#include <cinttypes>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

#include "db/db_impl/db_impl.h"
#include "db/dbformat.h"
#include "db/log_reader.h"
#include "db/write_batch_internal.h"
#include "file/filename.h"
#include "rocksdb/cache.h"
#include "rocksdb/file_checksum.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/table_properties.h"
#include "rocksdb/utilities/backupable_db.h"
#include "rocksdb/utilities/checkpoint.h"
#include "rocksdb/utilities/debug.h"
#include "rocksdb/utilities/options_util.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/write_buffer_manager.h"
#include "table/scoped_arena_iterator.h"
#include "table/sst_file_dumper.h"
#include "tools/ldb_cmd_impl.h"
#include "util/cast_util.h"
#include "util/coding.h"
#include "util/file_checksum_helper.h"
#include "util/stderr_logger.h"
#include "util/string_util.h"
#include "utilities/merge_operators.h"
#include "utilities/ttl/db_ttl_impl.h"

namespace ROCKSDB_NAMESPACE {

class FileChecksumGenCrc32c;
class FileChecksumGenCrc32cFactory;

const std::string LDBCommand::ARG_ENV_URI = "env_uri";
const std::string LDBCommand::ARG_FS_URI = "fs_uri";
const std::string LDBCommand::ARG_DB = "db";
const std::string LDBCommand::ARG_PATH = "path";
const std::string LDBCommand::ARG_SECONDARY_PATH = "secondary_path";
const std::string LDBCommand::ARG_HEX = "hex";
const std::string LDBCommand::ARG_KEY_HEX = "key_hex";
const std::string LDBCommand::ARG_VALUE_HEX = "value_hex";
const std::string LDBCommand::ARG_CF_NAME = "column_family";
const std::string LDBCommand::ARG_TTL = "ttl";
const std::string LDBCommand::ARG_TTL_START = "start_time";
const std::string LDBCommand::ARG_TTL_END = "end_time";
const std::string LDBCommand::ARG_TIMESTAMP = "timestamp";
const std::string LDBCommand::ARG_TRY_LOAD_OPTIONS = "try_load_options";
const std::string LDBCommand::ARG_DISABLE_CONSISTENCY_CHECKS =
    "disable_consistency_checks";
const std::string LDBCommand::ARG_IGNORE_UNKNOWN_OPTIONS =
    "ignore_unknown_options";
const std::string LDBCommand::ARG_FROM = "from";
const std::string LDBCommand::ARG_TO = "to";
const std::string LDBCommand::ARG_MAX_KEYS = "max_keys";
const std::string LDBCommand::ARG_BLOOM_BITS = "bloom_bits";
const std::string LDBCommand::ARG_FIX_PREFIX_LEN = "fix_prefix_len";
const std::string LDBCommand::ARG_COMPRESSION_TYPE = "compression_type";
const std::string LDBCommand::ARG_COMPRESSION_MAX_DICT_BYTES =
    "compression_max_dict_bytes";
const std::string LDBCommand::ARG_BLOCK_SIZE = "block_size";
const std::string LDBCommand::ARG_AUTO_COMPACTION = "auto_compaction";
const std::string LDBCommand::ARG_DB_WRITE_BUFFER_SIZE = "db_write_buffer_size";
const std::string LDBCommand::ARG_WRITE_BUFFER_SIZE = "write_buffer_size";
const std::string LDBCommand::ARG_FILE_SIZE = "file_size";
const std::string LDBCommand::ARG_CREATE_IF_MISSING = "create_if_missing";
const std::string LDBCommand::ARG_NO_VALUE = "no_value";

const char* LDBCommand::DELIM = " ==> ";

namespace {

void DumpWalFile(Options options, std::string wal_file, bool print_header,
                 bool print_values, bool is_write_committed,
                 LDBCommandExecuteResult* exec_state);

void DumpSstFile(Options options, std::string filename, bool output_hex,
                 bool show_properties);
};

LDBCommand* LDBCommand::InitFromCmdLineArgs(
    int argc, char const* const* argv, const Options& options,
    const LDBOptions& ldb_options,
    const std::vector<ColumnFamilyDescriptor>* column_families) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; i++) {
    args.push_back(argv[i]);
  }
  return InitFromCmdLineArgs(args, options, ldb_options, column_families,
                             SelectCommand);
}

/**
 * Parse the command-line arguments and create the appropriate LDBCommand2
 * instance.
 * The command line arguments must be in the following format:
 * ./ldb --db=PATH_TO_DB [--commonOpt1=commonOpt1Val] ..
 *        COMMAND <PARAM1> <PARAM2> ... [-cmdSpecificOpt1=cmdSpecificOpt1Val] ..
 * This is similar to the command line format used by HBaseClientTool.
 * Command name is not included in args.
 * Returns nullptr if the command-line cannot be parsed.
 */
LDBCommand* LDBCommand::InitFromCmdLineArgs(
    const std::vector<std::string>& args, const Options& options,
    const LDBOptions& ldb_options,
    const std::vector<ColumnFamilyDescriptor>* /*column_families*/,
    const std::function<LDBCommand*(const ParsedParams&)>& selector) {
  // --x=y command line arguments are added as x->y map entries in
  // parsed_params.option_map.
  //
  // Command-line arguments of the form --hex end up in this array as hex to
  // parsed_params.flags
  ParsedParams parsed_params;

  // Everything other than option_map and flags. Represents commands
  // and their parameters.  For eg: put key1 value1 go into this vector.
  std::vector<std::string> cmdTokens;

  const std::string OPTION_PREFIX = "--";

  for (const auto& arg : args) {
    if (arg[0] == '-' && arg[1] == '-'){
      std::vector<std::string> splits = StringSplit(arg, '=');
      // --option_name=option_value
      if (splits.size() == 2) {
        std::string optionKey = splits[0].substr(OPTION_PREFIX.size());
        parsed_params.option_map[optionKey] = splits[1];
      } else if (splits.size() == 1) {
        // --flag_name
        std::string optionKey = splits[0].substr(OPTION_PREFIX.size());
        parsed_params.flags.push_back(optionKey);
      } else {
        // --option_name=option_value, option_value contains '='
        std::string optionKey = splits[0].substr(OPTION_PREFIX.size());
        parsed_params.option_map[optionKey] =
            arg.substr(splits[0].length() + 1);
      }
    } else {
      cmdTokens.push_back(arg);
    }
  }

  if (cmdTokens.size() < 1) {
    fprintf(stderr, "Command not specified!");
    return nullptr;
  }

  parsed_params.cmd = cmdTokens[0];
  parsed_params.cmd_params.assign(cmdTokens.begin() + 1, cmdTokens.end());

  LDBCommand* command = selector(parsed_params);

  if (command) {
    command->SetDBOptions(options);
    command->SetLDBOptions(ldb_options);
  }
  return command;
}

LDBCommand* LDBCommand::SelectCommand(const ParsedParams& parsed_params) {
  if (parsed_params.cmd == GetCommand::Name()) {
    return new GetCommand(parsed_params.cmd_params, parsed_params.option_map,
                          parsed_params.flags);
  } else if (parsed_params.cmd == PutCommand::Name()) {
    return new PutCommand(parsed_params.cmd_params, parsed_params.option_map,
                          parsed_params.flags);
  } else if (parsed_params.cmd == BatchPutCommand::Name()) {
    return new BatchPutCommand(parsed_params.cmd_params,
                               parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == ScanCommand::Name()) {
    return new ScanCommand(parsed_params.cmd_params, parsed_params.option_map,
                           parsed_params.flags);
  } else if (parsed_params.cmd == DeleteCommand::Name()) {
    return new DeleteCommand(parsed_params.cmd_params, parsed_params.option_map,
                             parsed_params.flags);
  } else if (parsed_params.cmd == DeleteRangeCommand::Name()) {
    return new DeleteRangeCommand(parsed_params.cmd_params,
                                  parsed_params.option_map,
                                  parsed_params.flags);
  } else if (parsed_params.cmd == ApproxSizeCommand::Name()) {
    return new ApproxSizeCommand(parsed_params.cmd_params,
                                 parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == DBQuerierCommand::Name()) {
    return new DBQuerierCommand(parsed_params.cmd_params,
                                parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == CompactorCommand::Name()) {
    return new CompactorCommand(parsed_params.cmd_params,
                                parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == WALDumperCommand::Name()) {
    return new WALDumperCommand(parsed_params.cmd_params,
                                parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == ReduceDBLevelsCommand::Name()) {
    return new ReduceDBLevelsCommand(parsed_params.cmd_params,
                                     parsed_params.option_map,
                                     parsed_params.flags);
  } else if (parsed_params.cmd == ChangeCompactionStyleCommand::Name()) {
    return new ChangeCompactionStyleCommand(parsed_params.cmd_params,
                                            parsed_params.option_map,
                                            parsed_params.flags);
  } else if (parsed_params.cmd == DBDumperCommand::Name()) {
    return new DBDumperCommand(parsed_params.cmd_params,
                               parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == DBLoaderCommand::Name()) {
    return new DBLoaderCommand(parsed_params.cmd_params,
                               parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == ManifestDumpCommand::Name()) {
    return new ManifestDumpCommand(parsed_params.cmd_params,
                                   parsed_params.option_map,
                                   parsed_params.flags);
  } else if (parsed_params.cmd == FileChecksumDumpCommand::Name()) {
    return new FileChecksumDumpCommand(parsed_params.cmd_params,
                                       parsed_params.option_map,
                                       parsed_params.flags);
  } else if (parsed_params.cmd == GetPropertyCommand::Name()) {
    return new GetPropertyCommand(parsed_params.cmd_params,
                                  parsed_params.option_map,
                                  parsed_params.flags);
  } else if (parsed_params.cmd == ListColumnFamiliesCommand::Name()) {
    return new ListColumnFamiliesCommand(parsed_params.cmd_params,
                                         parsed_params.option_map,
                                         parsed_params.flags);
  } else if (parsed_params.cmd == CreateColumnFamilyCommand::Name()) {
    return new CreateColumnFamilyCommand(parsed_params.cmd_params,
                                         parsed_params.option_map,
                                         parsed_params.flags);
  } else if (parsed_params.cmd == DropColumnFamilyCommand::Name()) {
    return new DropColumnFamilyCommand(parsed_params.cmd_params,
                                       parsed_params.option_map,
                                       parsed_params.flags);
  } else if (parsed_params.cmd == DBFileDumperCommand::Name()) {
    return new DBFileDumperCommand(parsed_params.cmd_params,
                                   parsed_params.option_map,
                                   parsed_params.flags);
  } else if (parsed_params.cmd == DBLiveFilesMetadataDumperCommand::Name()) {
    return new DBLiveFilesMetadataDumperCommand(parsed_params.cmd_params,
                                                parsed_params.option_map,
                                                parsed_params.flags);
  } else if (parsed_params.cmd == InternalDumpCommand::Name()) {
    return new InternalDumpCommand(parsed_params.cmd_params,
                                   parsed_params.option_map,
                                   parsed_params.flags);
  } else if (parsed_params.cmd == CheckConsistencyCommand::Name()) {
    return new CheckConsistencyCommand(parsed_params.cmd_params,
                                       parsed_params.option_map,
                                       parsed_params.flags);
  } else if (parsed_params.cmd == CheckPointCommand::Name()) {
    return new CheckPointCommand(parsed_params.cmd_params,
                                 parsed_params.option_map,
                                 parsed_params.flags);
  } else if (parsed_params.cmd == RepairCommand::Name()) {
    return new RepairCommand(parsed_params.cmd_params, parsed_params.option_map,
                             parsed_params.flags);
  } else if (parsed_params.cmd == BackupCommand::Name()) {
    return new BackupCommand(parsed_params.cmd_params, parsed_params.option_map,
                             parsed_params.flags);
  } else if (parsed_params.cmd == RestoreCommand::Name()) {
    return new RestoreCommand(parsed_params.cmd_params,
                              parsed_params.option_map, parsed_params.flags);
  } else if (parsed_params.cmd == WriteExternalSstFilesCommand::Name()) {
    return new WriteExternalSstFilesCommand(parsed_params.cmd_params,
                                            parsed_params.option_map,
                                            parsed_params.flags);
  } else if (parsed_params.cmd == IngestExternalSstFilesCommand::Name()) {
    return new IngestExternalSstFilesCommand(parsed_params.cmd_params,
                                             parsed_params.option_map,
                                             parsed_params.flags);
  } else if (parsed_params.cmd == ListFileRangeDeletesCommand::Name()) {
    return new ListFileRangeDeletesCommand(parsed_params.option_map,
                                           parsed_params.flags);
  } else if (parsed_params.cmd == UnsafeRemoveSstFileCommand::Name()) {
    return new UnsafeRemoveSstFileCommand(parsed_params.cmd_params,
                                          parsed_params.option_map,
                                          parsed_params.flags);
  }
  return nullptr;
}

/* Run the command, and return the execute result. */
void LDBCommand::Run() {
  if (!exec_state_.IsNotStarted()) {
    return;
  }

  if (!options_.env || options_.env == Env::Default()) {
    Env* env = Env::Default();
    Status s = Env::CreateFromUri(config_options_, env_uri_, fs_uri_, &env,
                                  &env_guard_);
    if (!s.ok()) {
      fprintf(stderr, "%s\n", s.ToString().c_str());
      exec_state_ = LDBCommandExecuteResult::Failed(s.ToString());
      return;
    }
    options_.env = env;
  }

  if (db_ == nullptr && !NoDBOpen()) {
    OpenDB();
    if (exec_state_.IsFailed() && try_load_options_) {
      // We don't always return if there is a failure because a WAL file or
      // manifest file can be given to "dump" command so we should continue.
      // --try_load_options is not valid in those cases.
      return;
    }
  }

  // We'll intentionally proceed even if the DB can't be opened because users
  // can also specify a filename, not just a directory.
  DoCommand();

  if (exec_state_.IsNotStarted()) {
    exec_state_ = LDBCommandExecuteResult::Succeed("");
  }

  if (db_ != nullptr) {
    CloseDB();
  }
}

LDBCommand::LDBCommand(const std::map<std::string, std::string>& options,
                       const std::vector<std::string>& flags, bool is_read_only,
                       const std::vector<std::string>& valid_cmd_line_options)
    : db_(nullptr),
      db_ttl_(nullptr),
      is_read_only_(is_read_only),
      is_key_hex_(false),
      is_value_hex_(false),
      is_db_ttl_(false),
      timestamp_(false),
      try_load_options_(false),
      create_if_missing_(false),
      option_map_(options),
      flags_(flags),
      valid_cmd_line_options_(valid_cmd_line_options) {
  std::map<std::string, std::string>::const_iterator itr = options.find(ARG_DB);
  if (itr != options.end()) {
    db_path_ = itr->second;
  }

  itr = options.find(ARG_ENV_URI);
  if (itr != options.end()) {
    env_uri_ = itr->second;
  }

  itr = options.find(ARG_FS_URI);
  if (itr != options.end()) {
    fs_uri_ = itr->second;
  }

  itr = options.find(ARG_CF_NAME);
  if (itr != options.end()) {
    column_family_name_ = itr->second;
  } else {
    column_family_name_ = kDefaultColumnFamilyName;
  }

  itr = options.find(ARG_SECONDARY_PATH);
  secondary_path_ = "";
  if (itr != options.end()) {
    secondary_path_ = itr->second;
  }

  is_key_hex_ = IsKeyHex(options, flags);
  is_value_hex_ = IsValueHex(options, flags);
  is_db_ttl_ = IsFlagPresent(flags, ARG_TTL);
  timestamp_ = IsFlagPresent(flags, ARG_TIMESTAMP);
  try_load_options_ = IsFlagPresent(flags, ARG_TRY_LOAD_OPTIONS);
  force_consistency_checks_ =
      !IsFlagPresent(flags, ARG_DISABLE_CONSISTENCY_CHECKS);
  config_options_.ignore_unknown_options =
      IsFlagPresent(flags, ARG_IGNORE_UNKNOWN_OPTIONS);
}

void LDBCommand::OpenDB() {
  PrepareOptions();
  if (!exec_state_.IsNotStarted()) {
    return;
  }
  if (column_families_.empty() && !options_.merge_operator) {
    // No harm to add a general merge operator if it is not specified.
    options_.merge_operator = MergeOperators::CreateStringAppendOperator(':');
  }
  // Open the DB.
  Status st;
  std::vector<ColumnFamilyHandle*> handles_opened;
  if (is_db_ttl_) {
    // ldb doesn't yet support TTL DB with multiple column families
    if (!column_family_name_.empty() || !column_families_.empty()) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "ldb doesn't support TTL DB with multiple column families");
    }
    if (!secondary_path_.empty()) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Open as secondary is not supported for TTL DB yet.");
    }
    if (is_read_only_) {
      st = DBWithTTL::Open(options_, db_path_, &db_ttl_, 0, true);
    } else {
      st = DBWithTTL::Open(options_, db_path_, &db_ttl_);
    }
    db_ = db_ttl_;
  } else {
    if (is_read_only_ && secondary_path_.empty()) {
      if (column_families_.empty()) {
        st = DB::OpenForReadOnly(options_, db_path_, &db_);
      } else {
        st = DB::OpenForReadOnly(options_, db_path_, column_families_,
                                 &handles_opened, &db_);
      }
    } else {
      if (column_families_.empty()) {
        if (secondary_path_.empty()) {
          st = DB::Open(options_, db_path_, &db_);
        } else {
          st = DB::OpenAsSecondary(options_, db_path_, secondary_path_, &db_);
        }
      } else {
        if (secondary_path_.empty()) {
          st = DB::Open(options_, db_path_, column_families_, &handles_opened,
                        &db_);
        } else {
          st = DB::OpenAsSecondary(options_, db_path_, secondary_path_,
                                   column_families_, &handles_opened, &db_);
        }
      }
    }
  }
  if (!st.ok()) {
    std::string msg = st.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(msg);
  } else if (!handles_opened.empty()) {
    assert(handles_opened.size() == column_families_.size());
    bool found_cf_name = false;
    for (size_t i = 0; i < handles_opened.size(); i++) {
      cf_handles_[column_families_[i].name] = handles_opened[i];
      if (column_family_name_ == column_families_[i].name) {
        found_cf_name = true;
      }
    }
    if (!found_cf_name) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Non-existing column family " + column_family_name_);
      CloseDB();
    }
  } else {
    // We successfully opened DB in single column family mode.
    assert(column_families_.empty());
    if (column_family_name_ != kDefaultColumnFamilyName) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Non-existing column family " + column_family_name_);
      CloseDB();
    }
  }
}

void LDBCommand::CloseDB() {
  if (db_ != nullptr) {
    for (auto& pair : cf_handles_) {
      delete pair.second;
    }
    delete db_;
    db_ = nullptr;
  }
}

ColumnFamilyHandle* LDBCommand::GetCfHandle() {
  if (!cf_handles_.empty()) {
    auto it = cf_handles_.find(column_family_name_);
    if (it == cf_handles_.end()) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Cannot find column family " + column_family_name_);
    } else {
      return it->second;
    }
  }
  return db_->DefaultColumnFamily();
}

std::vector<std::string> LDBCommand::BuildCmdLineOptions(
    std::vector<std::string> options) {
  std::vector<std::string> ret = {ARG_ENV_URI,
                                  ARG_FS_URI,
                                  ARG_DB,
                                  ARG_SECONDARY_PATH,
                                  ARG_BLOOM_BITS,
                                  ARG_BLOCK_SIZE,
                                  ARG_AUTO_COMPACTION,
                                  ARG_COMPRESSION_TYPE,
                                  ARG_COMPRESSION_MAX_DICT_BYTES,
                                  ARG_WRITE_BUFFER_SIZE,
                                  ARG_FILE_SIZE,
                                  ARG_FIX_PREFIX_LEN,
                                  ARG_TRY_LOAD_OPTIONS,
                                  ARG_DISABLE_CONSISTENCY_CHECKS,
                                  ARG_IGNORE_UNKNOWN_OPTIONS,
                                  ARG_CF_NAME};
  ret.insert(ret.end(), options.begin(), options.end());
  return ret;
}

/**
 * Parses the specific integer option and fills in the value.
 * Returns true if the option is found.
 * Returns false if the option is not found or if there is an error parsing the
 * value.  If there is an error, the specified exec_state is also
 * updated.
 */
bool LDBCommand::ParseIntOption(
    const std::map<std::string, std::string>& /*options*/,
    const std::string& option, int& value,
    LDBCommandExecuteResult& exec_state) {
  std::map<std::string, std::string>::const_iterator itr =
      option_map_.find(option);
  if (itr != option_map_.end()) {
    try {
#if defined(CYGWIN)
      value = strtol(itr->second.c_str(), 0, 10);
#else
      value = std::stoi(itr->second);
#endif
      return true;
    } catch (const std::invalid_argument&) {
      exec_state =
          LDBCommandExecuteResult::Failed(option + " has an invalid value.");
    } catch (const std::out_of_range&) {
      exec_state = LDBCommandExecuteResult::Failed(
          option + " has a value out-of-range.");
    }
  }
  return false;
}

/**
 * Parses the specified option and fills in the value.
 * Returns true if the option is found.
 * Returns false otherwise.
 */
bool LDBCommand::ParseStringOption(
    const std::map<std::string, std::string>& /*options*/,
    const std::string& option, std::string* value) {
  auto itr = option_map_.find(option);
  if (itr != option_map_.end()) {
    *value = itr->second;
    return true;
  }
  return false;
}

void LDBCommand::OverrideBaseOptions() {
  options_.create_if_missing = false;

  int db_write_buffer_size;
  if (ParseIntOption(option_map_, ARG_DB_WRITE_BUFFER_SIZE,
                     db_write_buffer_size, exec_state_)) {
    if (db_write_buffer_size >= 0) {
      options_.db_write_buffer_size = db_write_buffer_size;
    } else {
      exec_state_ = LDBCommandExecuteResult::Failed(ARG_DB_WRITE_BUFFER_SIZE +
                                                    " must be >= 0.");
    }
  }

  if (options_.db_paths.size() == 0) {
    options_.db_paths.emplace_back(db_path_,
                                   std::numeric_limits<uint64_t>::max());
  }

  OverrideBaseCFOptions(static_cast<ColumnFamilyOptions*>(&options_));
}

void LDBCommand::OverrideBaseCFOptions(ColumnFamilyOptions* cf_opts) {
  BlockBasedTableOptions table_options;
  bool use_table_options = false;
  int bits;
  if (ParseIntOption(option_map_, ARG_BLOOM_BITS, bits, exec_state_)) {
    if (bits > 0) {
      use_table_options = true;
      table_options.filter_policy.reset(NewBloomFilterPolicy(bits));
    } else {
      exec_state_ =
          LDBCommandExecuteResult::Failed(ARG_BLOOM_BITS + " must be > 0.");
    }
  }

  int block_size;
  if (ParseIntOption(option_map_, ARG_BLOCK_SIZE, block_size, exec_state_)) {
    if (block_size > 0) {
      use_table_options = true;
      table_options.block_size = block_size;
    } else {
      exec_state_ =
          LDBCommandExecuteResult::Failed(ARG_BLOCK_SIZE + " must be > 0.");
    }
  }

  cf_opts->force_consistency_checks = force_consistency_checks_;
  if (use_table_options) {
    cf_opts->table_factory.reset(NewBlockBasedTableFactory(table_options));
  }

  auto itr = option_map_.find(ARG_AUTO_COMPACTION);
  if (itr != option_map_.end()) {
    cf_opts->disable_auto_compactions = !StringToBool(itr->second);
  }

  itr = option_map_.find(ARG_COMPRESSION_TYPE);
  if (itr != option_map_.end()) {
    std::string comp = itr->second;
    if (comp == "no") {
      cf_opts->compression = kNoCompression;
    } else if (comp == "snappy") {
      cf_opts->compression = kSnappyCompression;
    } else if (comp == "zlib") {
      cf_opts->compression = kZlibCompression;
    } else if (comp == "bzip2") {
      cf_opts->compression = kBZip2Compression;
    } else if (comp == "lz4") {
      cf_opts->compression = kLZ4Compression;
    } else if (comp == "lz4hc") {
      cf_opts->compression = kLZ4HCCompression;
    } else if (comp == "xpress") {
      cf_opts->compression = kXpressCompression;
    } else if (comp == "zstd") {
      cf_opts->compression = kZSTD;
    } else {
      // Unknown compression.
      exec_state_ =
          LDBCommandExecuteResult::Failed("Unknown compression level: " + comp);
    }
  }

  int compression_max_dict_bytes;
  if (ParseIntOption(option_map_, ARG_COMPRESSION_MAX_DICT_BYTES,
                     compression_max_dict_bytes, exec_state_)) {
    if (compression_max_dict_bytes >= 0) {
      cf_opts->compression_opts.max_dict_bytes = compression_max_dict_bytes;
    } else {
      exec_state_ = LDBCommandExecuteResult::Failed(
          ARG_COMPRESSION_MAX_DICT_BYTES + " must be >= 0.");
    }
  }

  int write_buffer_size;
  if (ParseIntOption(option_map_, ARG_WRITE_BUFFER_SIZE, write_buffer_size,
        exec_state_)) {
    if (write_buffer_size > 0) {
      cf_opts->write_buffer_size = write_buffer_size;
    } else {
      exec_state_ = LDBCommandExecuteResult::Failed(ARG_WRITE_BUFFER_SIZE +
                                                    " must be > 0.");
    }
  }

  int file_size;
  if (ParseIntOption(option_map_, ARG_FILE_SIZE, file_size, exec_state_)) {
    if (file_size > 0) {
      cf_opts->target_file_size_base = file_size;
    } else {
      exec_state_ =
          LDBCommandExecuteResult::Failed(ARG_FILE_SIZE + " must be > 0.");
    }
  }

  int fix_prefix_len;
  if (ParseIntOption(option_map_, ARG_FIX_PREFIX_LEN, fix_prefix_len,
                     exec_state_)) {
    if (fix_prefix_len > 0) {
      cf_opts->prefix_extractor.reset(
          NewFixedPrefixTransform(static_cast<size_t>(fix_prefix_len)));
    } else {
      exec_state_ =
          LDBCommandExecuteResult::Failed(ARG_FIX_PREFIX_LEN + " must be > 0.");
    }
  }
}

// First, initializes the options state using the OPTIONS file when enabled.
// Second, overrides the options according to the CLI arguments and the
// specific subcommand being run.
void LDBCommand::PrepareOptions() {
  if (!create_if_missing_ && try_load_options_) {
    config_options_.env = options_.env;
    Status s = LoadLatestOptions(config_options_, db_path_, &options_,
                                 &column_families_);
    if (!s.ok() && !s.IsNotFound()) {
      // Option file exists but load option file error.
      std::string msg = s.ToString();
      exec_state_ = LDBCommandExecuteResult::Failed(msg);
      db_ = nullptr;
      return;
    }
    if (!options_.wal_dir.empty()) {
      if (options_.env->FileExists(options_.wal_dir).IsNotFound()) {
        options_.wal_dir = db_path_;
        fprintf(
            stderr,
            "wal_dir loaded from the option file doesn't exist. Ignore it.\n");
      }
    }

    // If merge operator is not set, set a string append operator.
    for (auto& cf_entry : column_families_) {
      if (!cf_entry.options.merge_operator) {
        cf_entry.options.merge_operator =
            MergeOperators::CreateStringAppendOperator(':');
      }
    }
  }

  if (options_.env == Env::Default()) {
    options_.env = config_options_.env;
  }

  OverrideBaseOptions();
  if (exec_state_.IsFailed()) {
    return;
  }

  if (column_families_.empty()) {
    // Reads the MANIFEST to figure out what column families exist. In this
    // case, the option overrides from the CLI argument/specific subcommand
    // apply to all column families.
    std::vector<std::string> cf_list;
    Status st = DB::ListColumnFamilies(options_, db_path_, &cf_list);
    // It is possible the DB doesn't exist yet, for "create if not
    // existing" case. The failure is ignored here. We rely on DB::Open()
    // to give us the correct error message for problem with opening
    // existing DB.
    if (st.ok() && cf_list.size() > 1) {
      // Ignore single column family DB.
      for (auto cf_name : cf_list) {
        column_families_.emplace_back(cf_name, options_);
      }
    }
  } else {
    // We got column families from the OPTIONS file. In this case, the option
    // overrides from the CLI argument/specific subcommand only apply to the
    // column family specified by `--column_family_name`.
    auto column_families_iter =
        std::find_if(column_families_.begin(), column_families_.end(),
                     [this](const ColumnFamilyDescriptor& cf_desc) {
                       return cf_desc.name == column_family_name_;
                     });
    if (column_families_iter == column_families_.end()) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Non-existing column family " + column_family_name_);
      return;
    }
    OverrideBaseCFOptions(&column_families_iter->options);
  }
}

bool LDBCommand::ParseKeyValue(const std::string& line, std::string* key,
                               std::string* value, bool is_key_hex,
                               bool is_value_hex) {
  size_t pos = line.find(DELIM);
  if (pos != std::string::npos) {
    *key = line.substr(0, pos);
    *value = line.substr(pos + strlen(DELIM));
    if (is_key_hex) {
      *key = HexToString(*key);
    }
    if (is_value_hex) {
      *value = HexToString(*value);
    }
    return true;
  } else {
    return false;
  }
}

/**
 * Make sure that ONLY the command-line options and flags expected by this
 * command are specified on the command-line.  Extraneous options are usually
 * the result of user error.
 * Returns true if all checks pass.  Else returns false, and prints an
 * appropriate error msg to stderr.
 */
bool LDBCommand::ValidateCmdLineOptions() {
  for (std::map<std::string, std::string>::const_iterator itr =
           option_map_.begin();
       itr != option_map_.end(); ++itr) {
    if (std::find(valid_cmd_line_options_.begin(),
                  valid_cmd_line_options_.end(),
                  itr->first) == valid_cmd_line_options_.end()) {
      fprintf(stderr, "Invalid command-line option %s\n", itr->first.c_str());
      return false;
    }
  }

  for (std::vector<std::string>::const_iterator itr = flags_.begin();
       itr != flags_.end(); ++itr) {
    if (std::find(valid_cmd_line_options_.begin(),
                  valid_cmd_line_options_.end(),
                  *itr) == valid_cmd_line_options_.end()) {
      fprintf(stderr, "Invalid command-line flag %s\n", itr->c_str());
      return false;
    }
  }

  if (!NoDBOpen() && option_map_.find(ARG_DB) == option_map_.end() &&
      option_map_.find(ARG_PATH) == option_map_.end()) {
    fprintf(stderr, "Either %s or %s must be specified.\n", ARG_DB.c_str(),
            ARG_PATH.c_str());
    return false;
  }

  return true;
}

std::string LDBCommand::HexToString(const std::string& str) {
  std::string result;
  std::string::size_type len = str.length();
  if (len < 2 || str[0] != '0' || str[1] != 'x') {
    fprintf(stderr, "Invalid hex input %s.  Must start with 0x\n", str.c_str());
    throw "Invalid hex input";
  }
  if (!Slice(str.data() + 2, len - 2).DecodeHex(&result)) {
    throw "Invalid hex input";
  }
  return result;
}

std::string LDBCommand::StringToHex(const std::string& str) {
  std::string result("0x");
  result.append(Slice(str).ToString(true));
  return result;
}

std::string LDBCommand::PrintKeyValue(const std::string& key,
                                      const std::string& value, bool is_key_hex,
                                      bool is_value_hex) {
  std::string result;
  result.append(is_key_hex ? StringToHex(key) : key);
  result.append(DELIM);
  result.append(is_value_hex ? StringToHex(value) : value);
  return result;
}

std::string LDBCommand::PrintKeyValue(const std::string& key,
                                      const std::string& value, bool is_hex) {
  return PrintKeyValue(key, value, is_hex, is_hex);
}

std::string LDBCommand::HelpRangeCmdArgs() {
  std::ostringstream str_stream;
  str_stream << " ";
  str_stream << "[--" << ARG_FROM << "] ";
  str_stream << "[--" << ARG_TO << "] ";
  return str_stream.str();
}

bool LDBCommand::IsKeyHex(const std::map<std::string, std::string>& options,
                          const std::vector<std::string>& flags) {
  return (IsFlagPresent(flags, ARG_HEX) || IsFlagPresent(flags, ARG_KEY_HEX) ||
          ParseBooleanOption(options, ARG_HEX, false) ||
          ParseBooleanOption(options, ARG_KEY_HEX, false));
}

bool LDBCommand::IsValueHex(const std::map<std::string, std::string>& options,
                            const std::vector<std::string>& flags) {
  return (IsFlagPresent(flags, ARG_HEX) ||
          IsFlagPresent(flags, ARG_VALUE_HEX) ||
          ParseBooleanOption(options, ARG_HEX, false) ||
          ParseBooleanOption(options, ARG_VALUE_HEX, false));
}

bool LDBCommand::ParseBooleanOption(
    const std::map<std::string, std::string>& options,
    const std::string& option, bool default_val) {
  std::map<std::string, std::string>::const_iterator itr = options.find(option);
  if (itr != options.end()) {
    std::string option_val = itr->second;
    return StringToBool(itr->second);
  }
  return default_val;
}

bool LDBCommand::StringToBool(std::string val) {
  std::transform(val.begin(), val.end(), val.begin(),
                 [](char ch) -> char { return (char)::tolower(ch); });

  if (val == "true") {
    return true;
  } else if (val == "false") {
    return false;
  } else {
    throw "Invalid value for boolean argument";
  }
}

CompactorCommand::CompactorCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_FROM, ARG_TO, ARG_HEX, ARG_KEY_HEX,
                                      ARG_VALUE_HEX, ARG_TTL})),
      null_from_(true),
      null_to_(true) {
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_FROM);
  if (itr != options.end()) {
    null_from_ = false;
    from_ = itr->second;
  }

  itr = options.find(ARG_TO);
  if (itr != options.end()) {
    null_to_ = false;
    to_ = itr->second;
  }

  if (is_key_hex_) {
    if (!null_from_) {
      from_ = HexToString(from_);
    }
    if (!null_to_) {
      to_ = HexToString(to_);
    }
  }
}

void CompactorCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(CompactorCommand::Name());
  ret.append(HelpRangeCmdArgs());
  ret.append("\n");
}

void CompactorCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  Slice* begin = nullptr;
  Slice* end = nullptr;
  if (!null_from_) {
    begin = new Slice(from_);
  }
  if (!null_to_) {
    end = new Slice(to_);
  }

  CompactRangeOptions cro;
  cro.bottommost_level_compaction = BottommostLevelCompaction::kForceOptimized;

  Status s = db_->CompactRange(cro, GetCfHandle(), begin, end);
  if (!s.ok()) {
    std::stringstream oss;
    oss << "Compaction failed: " << s.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  } else {
    exec_state_ = LDBCommandExecuteResult::Succeed("");
  }

  delete begin;
  delete end;
}

// ---------------------------------------------------------------------------
const std::string DBLoaderCommand::ARG_DISABLE_WAL = "disable_wal";
const std::string DBLoaderCommand::ARG_BULK_LOAD = "bulk_load";
const std::string DBLoaderCommand::ARG_COMPACT = "compact";

DBLoaderCommand::DBLoaderCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, false,
          BuildCmdLineOptions({ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX, ARG_FROM,
                               ARG_TO, ARG_CREATE_IF_MISSING, ARG_DISABLE_WAL,
                               ARG_BULK_LOAD, ARG_COMPACT})),
      disable_wal_(false),
      bulk_load_(false),
      compact_(false) {
  create_if_missing_ = IsFlagPresent(flags, ARG_CREATE_IF_MISSING);
  disable_wal_ = IsFlagPresent(flags, ARG_DISABLE_WAL);
  bulk_load_ = IsFlagPresent(flags, ARG_BULK_LOAD);
  compact_ = IsFlagPresent(flags, ARG_COMPACT);
}

void DBLoaderCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DBLoaderCommand::Name());
  ret.append(" [--" + ARG_CREATE_IF_MISSING + "]");
  ret.append(" [--" + ARG_DISABLE_WAL + "]");
  ret.append(" [--" + ARG_BULK_LOAD + "]");
  ret.append(" [--" + ARG_COMPACT + "]");
  ret.append("\n");
}

void DBLoaderCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  options_.create_if_missing = create_if_missing_;
  if (bulk_load_) {
    options_.PrepareForBulkLoad();
  }
}

void DBLoaderCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  WriteOptions write_options;
  if (disable_wal_) {
    write_options.disableWAL = true;
  }

  int bad_lines = 0;
  std::string line;
  // prefer ifstream getline performance vs that from std::cin istream
  std::ifstream ifs_stdin("/dev/stdin");
  std::istream* istream_p = ifs_stdin.is_open() ? &ifs_stdin : &std::cin;
  Status s;
  while (s.ok() && getline(*istream_p, line, '\n')) {
    std::string key;
    std::string value;
    if (ParseKeyValue(line, &key, &value, is_key_hex_, is_value_hex_)) {
      s = db_->Put(write_options, GetCfHandle(), Slice(key), Slice(value));
    } else if (0 == line.find("Keys in range:")) {
      // ignore this line
    } else if (0 == line.find("Created bg thread 0x")) {
      // ignore this line
    } else {
      bad_lines ++;
    }
  }

  if (bad_lines > 0) {
    std::cout << "Warning: " << bad_lines << " bad lines ignored." << std::endl;
  }
  if (!s.ok()) {
    std::stringstream oss;
    oss << "Load failed: " << s.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  }
  if (compact_ && s.ok()) {
    s = db_->CompactRange(CompactRangeOptions(), GetCfHandle(), nullptr,
                          nullptr);
  }
  if (!s.ok()) {
    std::stringstream oss;
    oss << "Compaction failed: " << s.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  }
}

// ----------------------------------------------------------------------------

namespace {

void DumpManifestFile(Options options, std::string file, bool verbose, bool hex,
                      bool json) {
  EnvOptions sopt;
  std::string dbname("dummy");
  std::shared_ptr<Cache> tc(NewLRUCache(options.max_open_files - 10,
                                        options.table_cache_numshardbits));
  // Notice we are using the default options not through SanitizeOptions(),
  // if VersionSet::DumpManifest() depends on any option done by
  // SanitizeOptions(), we need to initialize it manually.
  options.db_paths.emplace_back("dummy", 0);
  options.num_levels = 64;
  WriteController wc(options.delayed_write_rate);
  WriteBufferManager wb(options.db_write_buffer_size);
  ImmutableDBOptions immutable_db_options(options);
  VersionSet versions(dbname, &immutable_db_options, sopt, tc.get(), &wb, &wc,
                      /*block_cache_tracer=*/nullptr, /*io_tracer=*/nullptr,
                      /*db_session_id*/ "");
  Status s = versions.DumpManifest(options, file, verbose, hex, json);
  if (!s.ok()) {
    fprintf(stderr, "Error in processing file %s %s\n", file.c_str(),
            s.ToString().c_str());
  }
}

}  // namespace

const std::string ManifestDumpCommand::ARG_VERBOSE = "verbose";
const std::string ManifestDumpCommand::ARG_JSON = "json";
const std::string ManifestDumpCommand::ARG_PATH = "path";

void ManifestDumpCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ManifestDumpCommand::Name());
  ret.append(" [--" + ARG_VERBOSE + "]");
  ret.append(" [--" + ARG_JSON + "]");
  ret.append(" [--" + ARG_PATH + "=<path_to_manifest_file>]");
  ret.append("\n");
}

ManifestDumpCommand::ManifestDumpCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, false,
          BuildCmdLineOptions({ARG_VERBOSE, ARG_PATH, ARG_HEX, ARG_JSON})),
      verbose_(false),
      json_(false),
      path_("") {
  verbose_ = IsFlagPresent(flags, ARG_VERBOSE);
  json_ = IsFlagPresent(flags, ARG_JSON);

  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_PATH);
  if (itr != options.end()) {
    path_ = itr->second;
    if (path_.empty()) {
      exec_state_ = LDBCommandExecuteResult::Failed("--path: missing pathname");
    }
  }
}

void ManifestDumpCommand::DoCommand() {

  std::string manifestfile;

  if (!path_.empty()) {
    manifestfile = path_;
  } else {
    // We need to find the manifest file by searching the directory
    // containing the db for files of the form MANIFEST_[0-9]+

    std::vector<std::string> files;
    Status s = options_.env->GetChildren(db_path_, &files);
    if (!s.ok()) {
      std::string err_msg = s.ToString();
      err_msg.append(": Failed to list the content of ");
      err_msg.append(db_path_);
      exec_state_ = LDBCommandExecuteResult::Failed(err_msg);
      return;
    }
    const std::string kManifestNamePrefix = "MANIFEST-";
    std::string matched_file;
#ifdef OS_WIN
    const char kPathDelim = '\\';
#else
    const char kPathDelim = '/';
#endif
    for (const auto& file_path : files) {
      // Some Env::GetChildren() return absolute paths. Some directories' path
      // end with path delim, e.g. '/' or '\\'.
      size_t pos = file_path.find_last_of(kPathDelim);
      if (pos == file_path.size() - 1) {
        continue;
      }
      std::string fname;
      if (pos != std::string::npos) {
        // Absolute path.
        fname.assign(file_path, pos + 1, file_path.size() - pos - 1);
      } else {
        fname = file_path;
      }
      uint64_t file_num = 0;
      FileType file_type = kWalFile;  // Just for initialization
      if (ParseFileName(fname, &file_num, &file_type) &&
          file_type == kDescriptorFile) {
        if (!matched_file.empty()) {
          exec_state_ = LDBCommandExecuteResult::Failed(
              "Multiple MANIFEST files found; use --path to select one");
          return;
        } else {
          matched_file.swap(fname);
        }
      }
    }
    if (matched_file.empty()) {
      std::string err_msg("No MANIFEST found in ");
      err_msg.append(db_path_);
      exec_state_ = LDBCommandExecuteResult::Failed(err_msg);
      return;
    }
    if (db_path_[db_path_.length() - 1] != '/') {
      db_path_.append("/");
    }
    manifestfile = db_path_ + matched_file;
  }

  if (verbose_) {
    fprintf(stdout, "Processing Manifest file %s\n", manifestfile.c_str());
  }

  DumpManifestFile(options_, manifestfile, verbose_, is_key_hex_, json_);

  if (verbose_) {
    fprintf(stdout, "Processing Manifest file %s done\n", manifestfile.c_str());
  }
}

// ----------------------------------------------------------------------------
namespace {

Status GetLiveFilesChecksumInfoFromVersionSet(Options options,
                                              const std::string& db_path,
                                              FileChecksumList* checksum_list) {
  EnvOptions sopt;
  Status s;
  std::string dbname(db_path);
  std::shared_ptr<Cache> tc(NewLRUCache(options.max_open_files - 10,
                                        options.table_cache_numshardbits));
  // Notice we are using the default options not through SanitizeOptions(),
  // if VersionSet::GetLiveFilesChecksumInfo depends on any option done by
  // SanitizeOptions(), we need to initialize it manually.
  options.db_paths.emplace_back(db_path, 0);
  options.num_levels = 64;
  WriteController wc(options.delayed_write_rate);
  WriteBufferManager wb(options.db_write_buffer_size);
  ImmutableDBOptions immutable_db_options(options);
  VersionSet versions(dbname, &immutable_db_options, sopt, tc.get(), &wb, &wc,
                      /*block_cache_tracer=*/nullptr, /*io_tracer=*/nullptr,
                      /*db_session_id*/ "");
  std::vector<std::string> cf_name_list;
  s = versions.ListColumnFamilies(&cf_name_list, db_path,
                                  immutable_db_options.fs.get());
  if (s.ok()) {
    std::vector<ColumnFamilyDescriptor> cf_list;
    for (const auto& name : cf_name_list) {
      cf_list.emplace_back(name, ColumnFamilyOptions(options));
    }
    s = versions.Recover(cf_list, true);
  }
  if (s.ok()) {
    s = versions.GetLiveFilesChecksumInfo(checksum_list);
  }
  return s;
}

}  // namespace

const std::string FileChecksumDumpCommand::ARG_PATH = "path";

void FileChecksumDumpCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(FileChecksumDumpCommand::Name());
  ret.append(" [--" + ARG_PATH + "=<path_to_manifest_file>]");
  ret.append("\n");
}

FileChecksumDumpCommand::FileChecksumDumpCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_PATH, ARG_HEX})),
      path_("") {
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_PATH);
  if (itr != options.end()) {
    path_ = itr->second;
    if (path_.empty()) {
      exec_state_ = LDBCommandExecuteResult::Failed("--path: missing pathname");
    }
  }
  is_checksum_hex_ = IsFlagPresent(flags, ARG_HEX);
}

void FileChecksumDumpCommand::DoCommand() {
  // print out the checksum information in the following format:
  //  sst file number, checksum function name, checksum value
  //  sst file number, checksum function name, checksum value
  //  ......

  std::unique_ptr<FileChecksumList> checksum_list(NewFileChecksumList());
  Status s = GetLiveFilesChecksumInfoFromVersionSet(options_, db_path_,
                                                    checksum_list.get());
  if (s.ok() && checksum_list != nullptr) {
    std::vector<uint64_t> file_numbers;
    std::vector<std::string> checksums;
    std::vector<std::string> checksum_func_names;
    s = checksum_list->GetAllFileChecksums(&file_numbers, &checksums,
                                           &checksum_func_names);
    if (s.ok()) {
      for (size_t i = 0; i < file_numbers.size(); i++) {
        assert(i < file_numbers.size());
        assert(i < checksums.size());
        assert(i < checksum_func_names.size());
        std::string checksum;
        if (is_checksum_hex_) {
          checksum = StringToHex(checksums[i]);
        } else {
          checksum = std::move(checksums[i]);
        }
        fprintf(stdout, "%" PRId64 ", %s, %s\n", file_numbers[i],
                checksum_func_names[i].c_str(), checksum.c_str());
      }
      fprintf(stdout, "Print SST file checksum information finished \n");
    }
  }

  if (!s.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(s.ToString());
  }
}

// ----------------------------------------------------------------------------

void GetPropertyCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(GetPropertyCommand::Name());
  ret.append(" <property_name>");
  ret.append("\n");
}

GetPropertyCommand::GetPropertyCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, BuildCmdLineOptions({})) {
  if (params.size() != 1) {
    exec_state_ =
        LDBCommandExecuteResult::Failed("property name must be specified");
  } else {
    property_ = params[0];
  }
}

void GetPropertyCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  std::map<std::string, std::string> value_map;
  std::string value;

  // Rather than having different ldb command for map properties vs. string
  // properties, we simply try Map property first. (This order only chosen
  // because I prefer the map-style output for
  // "rocksdb.aggregated-table-properties".)
  if (db_->GetMapProperty(GetCfHandle(), property_, &value_map)) {
    if (value_map.empty()) {
      fprintf(stdout, "%s: <empty map>\n", property_.c_str());
    } else {
      for (auto& e : value_map) {
        fprintf(stdout, "%s.%s: %s\n", property_.c_str(), e.first.c_str(),
                e.second.c_str());
      }
    }
  } else if (db_->GetProperty(GetCfHandle(), property_, &value)) {
    fprintf(stdout, "%s: %s\n", property_.c_str(), value.c_str());
  } else {
    exec_state_ =
        LDBCommandExecuteResult::Failed("failed to get property: " + property_);
  }
}

// ----------------------------------------------------------------------------

void ListColumnFamiliesCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ListColumnFamiliesCommand::Name());
  ret.append("\n");
}

ListColumnFamiliesCommand::ListColumnFamiliesCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false, BuildCmdLineOptions({})) {}

void ListColumnFamiliesCommand::DoCommand() {
  std::vector<std::string> column_families;
  Status s = DB::ListColumnFamilies(options_, db_path_, &column_families);
  if (!s.ok()) {
    fprintf(stderr, "Error in processing db %s %s\n", db_path_.c_str(),
            s.ToString().c_str());
  } else {
    fprintf(stdout, "Column families in %s: \n{", db_path_.c_str());
    bool first = true;
    for (auto cf : column_families) {
      if (!first) {
        fprintf(stdout, ", ");
      }
      first = false;
      fprintf(stdout, "%s", cf.c_str());
    }
    fprintf(stdout, "}\n");
  }
}

void CreateColumnFamilyCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(CreateColumnFamilyCommand::Name());
  ret.append(" --db=<db_path> <new_column_family_name>");
  ret.append("\n");
}

CreateColumnFamilyCommand::CreateColumnFamilyCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, {ARG_DB}) {
  if (params.size() != 1) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "new column family name must be specified");
  } else {
    new_cf_name_ = params[0];
  }
}

void CreateColumnFamilyCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  ColumnFamilyHandle* new_cf_handle = nullptr;
  Status st = db_->CreateColumnFamily(options_, new_cf_name_, &new_cf_handle);
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Fail to create new column family: " + st.ToString());
  }
  delete new_cf_handle;
  CloseDB();
}

void DropColumnFamilyCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DropColumnFamilyCommand::Name());
  ret.append(" --db=<db_path> <column_family_name_to_drop>");
  ret.append("\n");
}

DropColumnFamilyCommand::DropColumnFamilyCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, {ARG_DB}) {
  if (params.size() != 1) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "The name of column family to drop must be specified");
  } else {
    cf_name_to_drop_ = params[0];
  }
}

void DropColumnFamilyCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  auto iter = cf_handles_.find(cf_name_to_drop_);
  if (iter == cf_handles_.end()) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Column family: " + cf_name_to_drop_ + " doesn't exist in db.");
    return;
  }
  ColumnFamilyHandle* cf_handle_to_drop = iter->second;
  Status st = db_->DropColumnFamily(cf_handle_to_drop);
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Fail to drop column family: " + st.ToString());
  }
  CloseDB();
}

// ----------------------------------------------------------------------------
namespace {

// This function only called when it's the sane case of >1 buckets in time-range
// Also called only when timekv falls between ttl_start and ttl_end provided
void IncBucketCounts(std::vector<uint64_t>& bucket_counts, int ttl_start,
                     int time_range, int bucket_size, int timekv,
                     int num_buckets) {
#ifdef NDEBUG
  (void)time_range;
  (void)num_buckets;
#endif
  assert(time_range > 0 && timekv >= ttl_start && bucket_size > 0 &&
    timekv < (ttl_start + time_range) && num_buckets > 1);
  int bucket = (timekv - ttl_start) / bucket_size;
  bucket_counts[bucket]++;
}

void PrintBucketCounts(const std::vector<uint64_t>& bucket_counts,
                       int ttl_start, int ttl_end, int bucket_size,
                       int num_buckets) {
  int time_point = ttl_start;
  for(int i = 0; i < num_buckets - 1; i++, time_point += bucket_size) {
    fprintf(stdout, "Keys in range %s to %s : %lu\n",
            TimeToHumanString(time_point).c_str(),
            TimeToHumanString(time_point + bucket_size).c_str(),
            (unsigned long)bucket_counts[i]);
  }
  fprintf(stdout, "Keys in range %s to %s : %lu\n",
          TimeToHumanString(time_point).c_str(),
          TimeToHumanString(ttl_end).c_str(),
          (unsigned long)bucket_counts[num_buckets - 1]);
}

}  // namespace

const std::string InternalDumpCommand::ARG_COUNT_ONLY = "count_only";
const std::string InternalDumpCommand::ARG_COUNT_DELIM = "count_delim";
const std::string InternalDumpCommand::ARG_STATS = "stats";
const std::string InternalDumpCommand::ARG_INPUT_KEY_HEX = "input_key_hex";

InternalDumpCommand::InternalDumpCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, true,
          BuildCmdLineOptions({ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX, ARG_FROM,
                               ARG_TO, ARG_MAX_KEYS, ARG_COUNT_ONLY,
                               ARG_COUNT_DELIM, ARG_STATS, ARG_INPUT_KEY_HEX})),
      has_from_(false),
      has_to_(false),
      max_keys_(-1),
      delim_("."),
      count_only_(false),
      count_delim_(false),
      print_stats_(false),
      is_input_key_hex_(false) {
  has_from_ = ParseStringOption(options, ARG_FROM, &from_);
  has_to_ = ParseStringOption(options, ARG_TO, &to_);

  ParseIntOption(options, ARG_MAX_KEYS, max_keys_, exec_state_);
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_COUNT_DELIM);
  if (itr != options.end()) {
    delim_ = itr->second;
    count_delim_ = true;
   // fprintf(stdout,"delim = %c\n",delim_[0]);
  } else {
    count_delim_ = IsFlagPresent(flags, ARG_COUNT_DELIM);
    delim_=".";
  }

  print_stats_ = IsFlagPresent(flags, ARG_STATS);
  count_only_ = IsFlagPresent(flags, ARG_COUNT_ONLY);
  is_input_key_hex_ = IsFlagPresent(flags, ARG_INPUT_KEY_HEX);

  if (is_input_key_hex_) {
    if (has_from_) {
      from_ = HexToString(from_);
    }
    if (has_to_) {
      to_ = HexToString(to_);
    }
  }
}

void InternalDumpCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(InternalDumpCommand::Name());
  ret.append(HelpRangeCmdArgs());
  ret.append(" [--" + ARG_INPUT_KEY_HEX + "]");
  ret.append(" [--" + ARG_MAX_KEYS + "=<N>]");
  ret.append(" [--" + ARG_COUNT_ONLY + "]");
  ret.append(" [--" + ARG_COUNT_DELIM + "=<char>]");
  ret.append(" [--" + ARG_STATS + "]");
  ret.append("\n");
}

void InternalDumpCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  if (print_stats_) {
    std::string stats;
    if (db_->GetProperty(GetCfHandle(), "rocksdb.stats", &stats)) {
      fprintf(stdout, "%s\n", stats.c_str());
    }
  }

  // Cast as DBImpl to get internal iterator
  std::vector<KeyVersion> key_versions;
  Status st = GetAllKeyVersions(db_, GetCfHandle(), from_, to_, max_keys_,
                                &key_versions);
  if (!st.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
    return;
  }
  std::string rtype1, rtype2, row, val;
  rtype2 = "";
  uint64_t c=0;
  uint64_t s1=0,s2=0;

  long long count = 0;
  for (auto& key_version : key_versions) {
    InternalKey ikey(key_version.user_key, key_version.sequence,
                     static_cast<ValueType>(key_version.type));
    if (has_to_ && ikey.user_key() == to_) {
      // GetAllKeyVersions() includes keys with user key `to_`, but idump has
      // traditionally excluded such keys.
      break;
    }
    ++count;
    int k;
    if (count_delim_) {
      rtype1 = "";
      s1=0;
      row = ikey.Encode().ToString();
      val = key_version.value;
      for(k=0;row[k]!='\x01' && row[k]!='\0';k++)
        s1++;
      for(k=0;val[k]!='\x01' && val[k]!='\0';k++)
        s1++;
      for(int j=0;row[j]!=delim_[0] && row[j]!='\0' && row[j]!='\x01';j++)
        rtype1+=row[j];
      if(rtype2.compare("") && rtype2.compare(rtype1)!=0) {
        fprintf(stdout, "%s => count:%" PRIu64 "\tsize:%" PRIu64 "\n",
                rtype2.c_str(), c, s2);
        c=1;
        s2=s1;
        rtype2 = rtype1;
      } else {
        c++;
        s2+=s1;
        rtype2=rtype1;
      }
    }

    if (!count_only_ && !count_delim_) {
      std::string key = ikey.DebugString(is_key_hex_);
      std::string value = Slice(key_version.value).ToString(is_value_hex_);
      std::cout << key << " => " << value << "\n";
    }

    // Terminate if maximum number of keys have been dumped
    if (max_keys_ > 0 && count >= max_keys_) break;
  }
  if(count_delim_) {
    fprintf(stdout, "%s => count:%" PRIu64 "\tsize:%" PRIu64 "\n",
            rtype2.c_str(), c, s2);
  } else {
    fprintf(stdout, "Internal keys in range: %lld\n", count);
  }
}

const std::string DBDumperCommand::ARG_COUNT_ONLY = "count_only";
const std::string DBDumperCommand::ARG_COUNT_DELIM = "count_delim";
const std::string DBDumperCommand::ARG_STATS = "stats";
const std::string DBDumperCommand::ARG_TTL_BUCKET = "bucket";

DBDumperCommand::DBDumperCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true,
                 BuildCmdLineOptions(
                     {ARG_TTL, ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX, ARG_FROM,
                      ARG_TO, ARG_MAX_KEYS, ARG_COUNT_ONLY, ARG_COUNT_DELIM,
                      ARG_STATS, ARG_TTL_START, ARG_TTL_END, ARG_TTL_BUCKET,
                      ARG_TIMESTAMP, ARG_PATH})),
      null_from_(true),
      null_to_(true),
      max_keys_(-1),
      count_only_(false),
      count_delim_(false),
      print_stats_(false) {
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_FROM);
  if (itr != options.end()) {
    null_from_ = false;
    from_ = itr->second;
  }

  itr = options.find(ARG_TO);
  if (itr != options.end()) {
    null_to_ = false;
    to_ = itr->second;
  }

  itr = options.find(ARG_MAX_KEYS);
  if (itr != options.end()) {
    try {
#if defined(CYGWIN)
      max_keys_ = strtol(itr->second.c_str(), 0, 10);
#else
      max_keys_ = std::stoi(itr->second);
#endif
    } catch (const std::invalid_argument&) {
      exec_state_ = LDBCommandExecuteResult::Failed(ARG_MAX_KEYS +
                                                    " has an invalid value");
    } catch (const std::out_of_range&) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          ARG_MAX_KEYS + " has a value out-of-range");
    }
  }
  itr = options.find(ARG_COUNT_DELIM);
  if (itr != options.end()) {
    delim_ = itr->second;
    count_delim_ = true;
  } else {
    count_delim_ = IsFlagPresent(flags, ARG_COUNT_DELIM);
    delim_=".";
  }

  print_stats_ = IsFlagPresent(flags, ARG_STATS);
  count_only_ = IsFlagPresent(flags, ARG_COUNT_ONLY);

  if (is_key_hex_) {
    if (!null_from_) {
      from_ = HexToString(from_);
    }
    if (!null_to_) {
      to_ = HexToString(to_);
    }
  }

  itr = options.find(ARG_PATH);
  if (itr != options.end()) {
    path_ = itr->second;
    if (db_path_.empty()) {
      db_path_ = path_;
    }
  }
}

void DBDumperCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DBDumperCommand::Name());
  ret.append(HelpRangeCmdArgs());
  ret.append(" [--" + ARG_TTL + "]");
  ret.append(" [--" + ARG_MAX_KEYS + "=<N>]");
  ret.append(" [--" + ARG_TIMESTAMP + "]");
  ret.append(" [--" + ARG_COUNT_ONLY + "]");
  ret.append(" [--" + ARG_COUNT_DELIM + "=<char>]");
  ret.append(" [--" + ARG_STATS + "]");
  ret.append(" [--" + ARG_TTL_BUCKET + "=<N>]");
  ret.append(" [--" + ARG_TTL_START + "=<N>:- is inclusive]");
  ret.append(" [--" + ARG_TTL_END + "=<N>:- is exclusive]");
  ret.append(" [--" + ARG_PATH + "=<path_to_a_file>]");
  ret.append("\n");
}

/**
 * Handles two separate cases:
 *
 * 1) --db is specified - just dump the database.
 *
 * 2) --path is specified - determine based on file extension what dumping
 *    function to call. Please note that we intentionally use the extension
 *    and avoid probing the file contents under the assumption that renaming
 *    the files is not a supported scenario.
 *
 */
void DBDumperCommand::DoCommand() {
  if (!db_) {
    assert(!path_.empty());
    std::string fileName = GetFileNameFromPath(path_);
    uint64_t number;
    FileType type;

    exec_state_ = LDBCommandExecuteResult::Succeed("");

    if (!ParseFileName(fileName, &number, &type)) {
      exec_state_ =
          LDBCommandExecuteResult::Failed("Can't parse file type: " + path_);
      return;
    }

    switch (type) {
      case kWalFile:
        // TODO(myabandeh): allow configuring is_write_commited
        DumpWalFile(options_, path_, /* print_header_ */ true,
                    /* print_values_ */ true, true /* is_write_commited */,
                    &exec_state_);
        break;
      case kTableFile:
        DumpSstFile(options_, path_, is_key_hex_, /* show_properties */ true);
        break;
      case kDescriptorFile:
        DumpManifestFile(options_, path_, /* verbose_ */ false, is_key_hex_,
                         /*  json_ */ false);
        break;
      default:
        exec_state_ = LDBCommandExecuteResult::Failed(
            "File type not supported: " + path_);
        break;
    }

  } else {
    DoDumpCommand();
  }
}

void DBDumperCommand::DoDumpCommand() {
  assert(nullptr != db_);
  assert(path_.empty());

  // Parse command line args
  uint64_t count = 0;
  if (print_stats_) {
    std::string stats;
    if (db_->GetProperty("rocksdb.stats", &stats)) {
      fprintf(stdout, "%s\n", stats.c_str());
    }
  }

  // Setup key iterator
  ReadOptions scan_read_opts;
  scan_read_opts.total_order_seek = true;
  Iterator* iter = db_->NewIterator(scan_read_opts, GetCfHandle());
  Status st = iter->status();
  if (!st.ok()) {
    exec_state_ =
        LDBCommandExecuteResult::Failed("Iterator error." + st.ToString());
  }

  if (!null_from_) {
    iter->Seek(from_);
  } else {
    iter->SeekToFirst();
  }

  int max_keys = max_keys_;
  int ttl_start;
  if (!ParseIntOption(option_map_, ARG_TTL_START, ttl_start, exec_state_)) {
    ttl_start = DBWithTTLImpl::kMinTimestamp;  // TTL introduction time
  }
  int ttl_end;
  if (!ParseIntOption(option_map_, ARG_TTL_END, ttl_end, exec_state_)) {
    ttl_end = DBWithTTLImpl::kMaxTimestamp;  // Max time allowed by TTL feature
  }
  if (ttl_end < ttl_start) {
    fprintf(stderr, "Error: End time can't be less than start time\n");
    delete iter;
    return;
  }
  int time_range = ttl_end - ttl_start;
  int bucket_size;
  if (!ParseIntOption(option_map_, ARG_TTL_BUCKET, bucket_size, exec_state_) ||
      bucket_size <= 0) {
    bucket_size = time_range; // Will have just 1 bucket by default
  }
  //cretaing variables for row count of each type
  std::string rtype1, rtype2, row, val;
  rtype2 = "";
  uint64_t c=0;
  uint64_t s1=0,s2=0;

  // At this point, bucket_size=0 => time_range=0
  int num_buckets = (bucket_size >= time_range)
                        ? 1
                        : ((time_range + bucket_size - 1) / bucket_size);
  std::vector<uint64_t> bucket_counts(num_buckets, 0);
  if (is_db_ttl_ && !count_only_ && timestamp_ && !count_delim_) {
    fprintf(stdout, "Dumping key-values from %s to %s\n",
            TimeToHumanString(ttl_start).c_str(),
            TimeToHumanString(ttl_end).c_str());
  }

  HistogramImpl vsize_hist;

  for (; iter->Valid(); iter->Next()) {
    int rawtime = 0;
    // If end marker was specified, we stop before it
    if (!null_to_ && (iter->key().ToString() >= to_))
      break;
    // Terminate if maximum number of keys have been dumped
    if (max_keys == 0)
      break;
    if (is_db_ttl_) {
      TtlIterator* it_ttl = static_cast_with_check<TtlIterator>(iter);
      rawtime = it_ttl->ttl_timestamp();
      if (rawtime < ttl_start || rawtime >= ttl_end) {
        continue;
      }
    }
    if (max_keys > 0) {
      --max_keys;
    }
    if (is_db_ttl_ && num_buckets > 1) {
      IncBucketCounts(bucket_counts, ttl_start, time_range, bucket_size,
                      rawtime, num_buckets);
    }
    ++count;
    if (count_delim_) {
      rtype1 = "";
      row = iter->key().ToString();
      val = iter->value().ToString();
      s1 = row.size()+val.size();
      for(int j=0;row[j]!=delim_[0] && row[j]!='\0';j++)
        rtype1+=row[j];
      if(rtype2.compare("") && rtype2.compare(rtype1)!=0) {
        fprintf(stdout, "%s => count:%" PRIu64 "\tsize:%" PRIu64 "\n",
                rtype2.c_str(), c, s2);
        c=1;
        s2=s1;
        rtype2 = rtype1;
      } else {
          c++;
          s2+=s1;
          rtype2=rtype1;
      }

    }

    if (count_only_) {
      vsize_hist.Add(iter->value().size());
    }

    if (!count_only_ && !count_delim_) {
      if (is_db_ttl_ && timestamp_) {
        fprintf(stdout, "%s ", TimeToHumanString(rawtime).c_str());
      }
      std::string str =
          PrintKeyValue(iter->key().ToString(), iter->value().ToString(),
                        is_key_hex_, is_value_hex_);
      fprintf(stdout, "%s\n", str.c_str());
    }
  }

  if (num_buckets > 1 && is_db_ttl_) {
    PrintBucketCounts(bucket_counts, ttl_start, ttl_end, bucket_size,
                      num_buckets);
  } else if(count_delim_) {
    fprintf(stdout, "%s => count:%" PRIu64 "\tsize:%" PRIu64 "\n",
            rtype2.c_str(), c, s2);
  } else {
    fprintf(stdout, "Keys in range: %" PRIu64 "\n", count);
  }

  if (count_only_) {
    fprintf(stdout, "Value size distribution: \n");
    fprintf(stdout, "%s\n", vsize_hist.ToString().c_str());
  }
  // Clean up
  delete iter;
}

const std::string ReduceDBLevelsCommand::ARG_NEW_LEVELS = "new_levels";
const std::string ReduceDBLevelsCommand::ARG_PRINT_OLD_LEVELS =
    "print_old_levels";

ReduceDBLevelsCommand::ReduceDBLevelsCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_NEW_LEVELS, ARG_PRINT_OLD_LEVELS})),
      old_levels_(1 << 7),
      new_levels_(-1),
      print_old_levels_(false) {
  ParseIntOption(option_map_, ARG_NEW_LEVELS, new_levels_, exec_state_);
  print_old_levels_ = IsFlagPresent(flags, ARG_PRINT_OLD_LEVELS);

  if(new_levels_ <= 0) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        " Use --" + ARG_NEW_LEVELS + " to specify a new level number\n");
  }
}

std::vector<std::string> ReduceDBLevelsCommand::PrepareArgs(
    const std::string& db_path, int new_levels, bool print_old_level) {
  std::vector<std::string> ret;
  ret.push_back("reduce_levels");
  ret.push_back("--" + ARG_DB + "=" + db_path);
  ret.push_back("--" + ARG_NEW_LEVELS + "=" +
                ROCKSDB_NAMESPACE::ToString(new_levels));
  if(print_old_level) {
    ret.push_back("--" + ARG_PRINT_OLD_LEVELS);
  }
  return ret;
}

void ReduceDBLevelsCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ReduceDBLevelsCommand::Name());
  ret.append(" --" + ARG_NEW_LEVELS + "=<New number of levels>");
  ret.append(" [--" + ARG_PRINT_OLD_LEVELS + "]");
  ret.append("\n");
}

void ReduceDBLevelsCommand::OverrideBaseCFOptions(
    ColumnFamilyOptions* cf_opts) {
  LDBCommand::OverrideBaseCFOptions(cf_opts);
  cf_opts->num_levels = old_levels_;
  cf_opts->max_bytes_for_level_multiplier_additional.resize(cf_opts->num_levels,
                                                            1);
  // Disable size compaction
  cf_opts->max_bytes_for_level_base = 1ULL << 50;
  cf_opts->max_bytes_for_level_multiplier = 1;
}

Status ReduceDBLevelsCommand::GetOldNumOfLevels(Options& opt,
    int* levels) {
  ImmutableDBOptions db_options(opt);
  EnvOptions soptions;
  std::shared_ptr<Cache> tc(
      NewLRUCache(opt.max_open_files - 10, opt.table_cache_numshardbits));
  const InternalKeyComparator cmp(opt.comparator);
  WriteController wc(opt.delayed_write_rate);
  WriteBufferManager wb(opt.db_write_buffer_size);
  VersionSet versions(db_path_, &db_options, soptions, tc.get(), &wb, &wc,
                      /*block_cache_tracer=*/nullptr, /*io_tracer=*/nullptr,
                      /*db_session_id*/ "");
  std::vector<ColumnFamilyDescriptor> dummy;
  ColumnFamilyDescriptor dummy_descriptor(kDefaultColumnFamilyName,
                                          ColumnFamilyOptions(opt));
  dummy.push_back(dummy_descriptor);
  // We rely the VersionSet::Recover to tell us the internal data structures
  // in the db. And the Recover() should never do any change
  // (like LogAndApply) to the manifest file.
  Status st = versions.Recover(dummy);
  if (!st.ok()) {
    return st;
  }
  int max = -1;
  auto default_cfd = versions.GetColumnFamilySet()->GetDefault();
  for (int i = 0; i < default_cfd->NumberLevels(); i++) {
    if (default_cfd->current()->storage_info()->NumLevelFiles(i)) {
      max = i;
    }
  }

  *levels = max + 1;
  return st;
}

void ReduceDBLevelsCommand::DoCommand() {
  if (new_levels_ <= 1) {
    exec_state_ =
        LDBCommandExecuteResult::Failed("Invalid number of levels.\n");
    return;
  }

  Status st;
  PrepareOptions();
  int old_level_num = -1;
  st = GetOldNumOfLevels(options_, &old_level_num);
  if (!st.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
    return;
  }

  if (print_old_levels_) {
    fprintf(stdout, "The old number of levels in use is %d\n", old_level_num);
  }

  if (old_level_num <= new_levels_) {
    return;
  }

  old_levels_ = old_level_num;

  OpenDB();
  if (exec_state_.IsFailed()) {
    return;
  }
  assert(db_ != nullptr);
  // Compact the whole DB to put all files to the highest level.
  fprintf(stdout, "Compacting the db...\n");
  st =
      db_->CompactRange(CompactRangeOptions(), GetCfHandle(), nullptr, nullptr);

  CloseDB();

  if (st.ok()) {
    EnvOptions soptions;
    st = VersionSet::ReduceNumberOfLevels(db_path_, &options_, soptions,
                                          new_levels_);
  }
  if (!st.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
    return;
  }
}

const std::string ChangeCompactionStyleCommand::ARG_OLD_COMPACTION_STYLE =
    "old_compaction_style";
const std::string ChangeCompactionStyleCommand::ARG_NEW_COMPACTION_STYLE =
    "new_compaction_style";

ChangeCompactionStyleCommand::ChangeCompactionStyleCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions(
                     {ARG_OLD_COMPACTION_STYLE, ARG_NEW_COMPACTION_STYLE})),
      old_compaction_style_(-1),
      new_compaction_style_(-1) {
  ParseIntOption(option_map_, ARG_OLD_COMPACTION_STYLE, old_compaction_style_,
    exec_state_);
  if (old_compaction_style_ != kCompactionStyleLevel &&
     old_compaction_style_ != kCompactionStyleUniversal) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Use --" + ARG_OLD_COMPACTION_STYLE + " to specify old compaction " +
        "style. Check ldb help for proper compaction style value.\n");
    return;
  }

  ParseIntOption(option_map_, ARG_NEW_COMPACTION_STYLE, new_compaction_style_,
    exec_state_);
  if (new_compaction_style_ != kCompactionStyleLevel &&
     new_compaction_style_ != kCompactionStyleUniversal) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Use --" + ARG_NEW_COMPACTION_STYLE + " to specify new compaction " +
        "style. Check ldb help for proper compaction style value.\n");
    return;
  }

  if (new_compaction_style_ == old_compaction_style_) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Old compaction style is the same as new compaction style. "
        "Nothing to do.\n");
    return;
  }

  if (old_compaction_style_ == kCompactionStyleUniversal &&
      new_compaction_style_ == kCompactionStyleLevel) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Convert from universal compaction to level compaction. "
        "Nothing to do.\n");
    return;
  }
}

void ChangeCompactionStyleCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ChangeCompactionStyleCommand::Name());
  ret.append(" --" + ARG_OLD_COMPACTION_STYLE + "=<Old compaction style: 0 " +
             "for level compaction, 1 for universal compaction>");
  ret.append(" --" + ARG_NEW_COMPACTION_STYLE + "=<New compaction style: 0 " +
             "for level compaction, 1 for universal compaction>");
  ret.append("\n");
}

void ChangeCompactionStyleCommand::OverrideBaseCFOptions(
    ColumnFamilyOptions* cf_opts) {
  LDBCommand::OverrideBaseCFOptions(cf_opts);
  if (old_compaction_style_ == kCompactionStyleLevel &&
      new_compaction_style_ == kCompactionStyleUniversal) {
    // In order to convert from level compaction to universal compaction, we
    // need to compact all data into a single file and move it to level 0.
    cf_opts->disable_auto_compactions = true;
    cf_opts->target_file_size_base = INT_MAX;
    cf_opts->target_file_size_multiplier = 1;
    cf_opts->max_bytes_for_level_base = INT_MAX;
    cf_opts->max_bytes_for_level_multiplier = 1;
  }
}

void ChangeCompactionStyleCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  // print db stats before we have made any change
  std::string property;
  std::string files_per_level;
  for (int i = 0; i < db_->NumberLevels(GetCfHandle()); i++) {
    db_->GetProperty(GetCfHandle(), "rocksdb.num-files-at-level" + ToString(i),
                     &property);

    // format print string
    char buf[100];
    snprintf(buf, sizeof(buf), "%s%s", (i ? "," : ""), property.c_str());
    files_per_level += buf;
  }
  fprintf(stdout, "files per level before compaction: %s\n",
          files_per_level.c_str());

  // manual compact into a single file and move the file to level 0
  CompactRangeOptions compact_options;
  compact_options.change_level = true;
  compact_options.target_level = 0;
  Status s =
      db_->CompactRange(compact_options, GetCfHandle(), nullptr, nullptr);
  if (!s.ok()) {
    std::stringstream oss;
    oss << "Compaction failed: " << s.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
    return;
  }

  // verify compaction result
  files_per_level = "";
  int num_files = 0;
  for (int i = 0; i < db_->NumberLevels(GetCfHandle()); i++) {
    db_->GetProperty(GetCfHandle(), "rocksdb.num-files-at-level" + ToString(i),
                     &property);

    // format print string
    char buf[100];
    snprintf(buf, sizeof(buf), "%s%s", (i ? "," : ""), property.c_str());
    files_per_level += buf;

    num_files = atoi(property.c_str());

    // level 0 should have only 1 file
    if (i == 0 && num_files != 1) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Number of db files at "
          "level 0 after compaction is " +
          ToString(num_files) + ", not 1.\n");
      return;
    }
    // other levels should have no file
    if (i > 0 && num_files != 0) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Number of db files at "
          "level " +
          ToString(i) + " after compaction is " + ToString(num_files) +
          ", not 0.\n");
      return;
    }
  }

  fprintf(stdout, "files per level after compaction: %s\n",
          files_per_level.c_str());
}

// ----------------------------------------------------------------------------

namespace {

struct StdErrReporter : public log::Reader::Reporter {
  void Corruption(size_t /*bytes*/, const Status& s) override {
    std::cerr << "Corruption detected in log file " << s.ToString() << "\n";
  }
};

class InMemoryHandler : public WriteBatch::Handler {
 public:
  InMemoryHandler(std::stringstream& row, bool print_values,
                  bool write_after_commit = false)
      : Handler(),
        row_(row),
        print_values_(print_values),
        write_after_commit_(write_after_commit) {}

  void commonPutMerge(const Slice& key, const Slice& value) {
    std::string k = LDBCommand::StringToHex(key.ToString());
    if (print_values_) {
      std::string v = LDBCommand::StringToHex(value.ToString());
      row_ << k << " : ";
      row_ << v << " ";
    } else {
      row_ << k << " ";
    }
  }

  Status PutCF(uint32_t cf, const Slice& key, const Slice& value) override {
    row_ << "PUT(" << cf << ") : ";
    commonPutMerge(key, value);
    return Status::OK();
  }

  Status MergeCF(uint32_t cf, const Slice& key, const Slice& value) override {
    row_ << "MERGE(" << cf << ") : ";
    commonPutMerge(key, value);
    return Status::OK();
  }

  Status MarkNoop(bool) override {
    row_ << "NOOP ";
    return Status::OK();
  }

  Status DeleteCF(uint32_t cf, const Slice& key) override {
    row_ << "DELETE(" << cf << ") : ";
    row_ << LDBCommand::StringToHex(key.ToString()) << " ";
    return Status::OK();
  }

  Status SingleDeleteCF(uint32_t cf, const Slice& key) override {
    row_ << "SINGLE_DELETE(" << cf << ") : ";
    row_ << LDBCommand::StringToHex(key.ToString()) << " ";
    return Status::OK();
  }

  Status DeleteRangeCF(uint32_t cf, const Slice& begin_key,
                       const Slice& end_key) override {
    row_ << "DELETE_RANGE(" << cf << ") : ";
    row_ << LDBCommand::StringToHex(begin_key.ToString()) << " ";
    row_ << LDBCommand::StringToHex(end_key.ToString()) << " ";
    return Status::OK();
  }

  Status MarkBeginPrepare(bool unprepare) override {
    row_ << "BEGIN_PREPARE(";
    row_ << (unprepare ? "true" : "false") << ") ";
    return Status::OK();
  }

  Status MarkEndPrepare(const Slice& xid) override {
    row_ << "END_PREPARE(";
    row_ << LDBCommand::StringToHex(xid.ToString()) << ") ";
    return Status::OK();
  }

  Status MarkRollback(const Slice& xid) override {
    row_ << "ROLLBACK(";
    row_ << LDBCommand::StringToHex(xid.ToString()) << ") ";
    return Status::OK();
  }

  Status MarkCommit(const Slice& xid) override {
    row_ << "COMMIT(";
    row_ << LDBCommand::StringToHex(xid.ToString()) << ") ";
    return Status::OK();
  }

  ~InMemoryHandler() override {}

 protected:
  bool WriteAfterCommit() const override { return write_after_commit_; }

 private:
  std::stringstream& row_;
  bool print_values_;
  bool write_after_commit_;
};

void DumpWalFile(Options options, std::string wal_file, bool print_header,
                 bool print_values, bool is_write_committed,
                 LDBCommandExecuteResult* exec_state) {
  const auto& fs = options.env->GetFileSystem();
  FileOptions soptions(options);
  std::unique_ptr<SequentialFileReader> wal_file_reader;
  Status status = SequentialFileReader::Create(fs, wal_file, soptions,
                                               &wal_file_reader, nullptr);
  if (!status.ok()) {
    if (exec_state) {
      *exec_state = LDBCommandExecuteResult::Failed("Failed to open WAL file " +
                                                    status.ToString());
    } else {
      std::cerr << "Error: Failed to open WAL file " << status.ToString()
                << std::endl;
    }
  } else {
    StdErrReporter reporter;
    uint64_t log_number;
    FileType type;

    // we need the log number, but ParseFilename expects dbname/NNN.log.
    std::string sanitized = wal_file;
    size_t lastslash = sanitized.rfind('/');
    if (lastslash != std::string::npos)
      sanitized = sanitized.substr(lastslash + 1);
    if (!ParseFileName(sanitized, &log_number, &type)) {
      // bogus input, carry on as best we can
      log_number = 0;
    }
    log::Reader reader(options.info_log, std::move(wal_file_reader), &reporter,
                       true /* checksum */, log_number);
    std::string scratch;
    WriteBatch batch;
    Slice record;
    std::stringstream row;
    if (print_header) {
      std::cout << "Sequence,Count,ByteSize,Physical Offset,Key(s)";
      if (print_values) {
        std::cout << " : value ";
      }
      std::cout << "\n";
    }
    while (status.ok() && reader.ReadRecord(&record, &scratch)) {
      row.str("");
      if (record.size() < WriteBatchInternal::kHeader) {
        reporter.Corruption(record.size(),
                            Status::Corruption("log record too small"));
      } else {
        status = WriteBatchInternal::SetContents(&batch, record);
        if (!status.ok()) {
          std::stringstream oss;
          oss << "Parsing write batch failed: " << status.ToString();
          if (exec_state) {
            *exec_state = LDBCommandExecuteResult::Failed(oss.str());
          } else {
            std::cerr << oss.str() << std::endl;
          }
          break;
        }
        row << WriteBatchInternal::Sequence(&batch) << ",";
        row << WriteBatchInternal::Count(&batch) << ",";
        row << WriteBatchInternal::ByteSize(&batch) << ",";
        row << reader.LastRecordOffset() << ",";
        InMemoryHandler handler(row, print_values, is_write_committed);
        status = batch.Iterate(&handler);
        if (!status.ok()) {
          if (exec_state) {
            std::stringstream oss;
            oss << "Print write batch error: " << status.ToString();
            *exec_state = LDBCommandExecuteResult::Failed(oss.str());
          }
          row << "error: " << status.ToString();
          break;
        }
        row << "\n";
      }
      std::cout << row.str();
    }
  }
}

}  // namespace

const std::string WALDumperCommand::ARG_WAL_FILE = "walfile";
const std::string WALDumperCommand::ARG_WRITE_COMMITTED = "write_committed";
const std::string WALDumperCommand::ARG_PRINT_VALUE = "print_value";
const std::string WALDumperCommand::ARG_PRINT_HEADER = "header";

WALDumperCommand::WALDumperCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true,
                 BuildCmdLineOptions({ARG_WAL_FILE, ARG_WRITE_COMMITTED,
                                      ARG_PRINT_HEADER, ARG_PRINT_VALUE})),
      print_header_(false),
      print_values_(false),
      is_write_committed_(false) {
  wal_file_.clear();

  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_WAL_FILE);
  if (itr != options.end()) {
    wal_file_ = itr->second;
  }


  print_header_ = IsFlagPresent(flags, ARG_PRINT_HEADER);
  print_values_ = IsFlagPresent(flags, ARG_PRINT_VALUE);
  is_write_committed_ = ParseBooleanOption(options, ARG_WRITE_COMMITTED, true);

  if (wal_file_.empty()) {
    exec_state_ = LDBCommandExecuteResult::Failed("Argument " + ARG_WAL_FILE +
                                                  " must be specified.");
  }
}

void WALDumperCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(WALDumperCommand::Name());
  ret.append(" --" + ARG_WAL_FILE + "=<write_ahead_log_file_path>");
  ret.append(" [--" + ARG_PRINT_HEADER + "] ");
  ret.append(" [--" + ARG_PRINT_VALUE + "] ");
  ret.append(" [--" + ARG_WRITE_COMMITTED + "=true|false] ");
  ret.append("\n");
}

void WALDumperCommand::DoCommand() {
  DumpWalFile(options_, wal_file_, print_header_, print_values_,
              is_write_committed_, &exec_state_);
}

// ----------------------------------------------------------------------------

GetCommand::GetCommand(const std::vector<std::string>& params,
                       const std::map<std::string, std::string>& options,
                       const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, true,
          BuildCmdLineOptions({ARG_TTL, ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX})) {
  if (params.size() != 1) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "<key> must be specified for the get command");
  } else {
    key_ = params.at(0);
  }

  if (is_key_hex_) {
    key_ = HexToString(key_);
  }
}

void GetCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(GetCommand::Name());
  ret.append(" <key>");
  ret.append(" [--" + ARG_TTL + "]");
  ret.append("\n");
}

void GetCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  std::string value;
  Status st = db_->Get(ReadOptions(), GetCfHandle(), key_, &value);
  if (st.ok()) {
    fprintf(stdout, "%s\n",
              (is_value_hex_ ? StringToHex(value) : value).c_str());
  } else {
    std::stringstream oss;
    oss << "Get failed: " << st.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  }
}

// ----------------------------------------------------------------------------

ApproxSizeCommand::ApproxSizeCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true,
                 BuildCmdLineOptions(
                     {ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX, ARG_FROM, ARG_TO})) {
  if (options.find(ARG_FROM) != options.end()) {
    start_key_ = options.find(ARG_FROM)->second;
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(
        ARG_FROM + " must be specified for approxsize command");
    return;
  }

  if (options.find(ARG_TO) != options.end()) {
    end_key_ = options.find(ARG_TO)->second;
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(
        ARG_TO + " must be specified for approxsize command");
    return;
  }

  if (is_key_hex_) {
    start_key_ = HexToString(start_key_);
    end_key_ = HexToString(end_key_);
  }
}

void ApproxSizeCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ApproxSizeCommand::Name());
  ret.append(HelpRangeCmdArgs());
  ret.append("\n");
}

void ApproxSizeCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Range ranges[1];
  ranges[0] = Range(start_key_, end_key_);
  uint64_t sizes[1];
  Status s = db_->GetApproximateSizes(GetCfHandle(), ranges, 1, sizes);
  if (!s.ok()) {
    std::stringstream oss;
    oss << "ApproximateSize failed: " << s.ToString();
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  } else {
    fprintf(stdout, "%lu\n", (unsigned long)sizes[0]);
  }
}

// ----------------------------------------------------------------------------

BatchPutCommand::BatchPutCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_TTL, ARG_HEX, ARG_KEY_HEX,
                                      ARG_VALUE_HEX, ARG_CREATE_IF_MISSING})) {
  if (params.size() < 2) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "At least one <key> <value> pair must be specified batchput.");
  } else if (params.size() % 2 != 0) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Equal number of <key>s and <value>s must be specified for batchput.");
  } else {
    for (size_t i = 0; i < params.size(); i += 2) {
      std::string key = params.at(i);
      std::string value = params.at(i + 1);
      key_values_.push_back(std::pair<std::string, std::string>(
          is_key_hex_ ? HexToString(key) : key,
          is_value_hex_ ? HexToString(value) : value));
    }
  }
  create_if_missing_ = IsFlagPresent(flags_, ARG_CREATE_IF_MISSING);
}

void BatchPutCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(BatchPutCommand::Name());
  ret.append(" <key> <value> [<key> <value>] [..]");
  ret.append(" [--" + ARG_CREATE_IF_MISSING + "]");
  ret.append(" [--" + ARG_TTL + "]");
  ret.append("\n");
}

void BatchPutCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  WriteBatch batch;

  Status st;
  std::stringstream oss;
  for (std::vector<std::pair<std::string, std::string>>::const_iterator itr =
           key_values_.begin();
       itr != key_values_.end(); ++itr) {
    st = batch.Put(GetCfHandle(), itr->first, itr->second);
    if (!st.ok()) {
      oss << "Put to write batch failed: " << itr->first << "=>" << itr->second
          << " error: " << st.ToString();
      break;
    }
  }
  if (st.ok()) {
    st = db_->Write(WriteOptions(), &batch);
    if (!st.ok()) {
      oss << "Write failed: " << st.ToString();
    }
  }
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  }
}

void BatchPutCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  options_.create_if_missing = create_if_missing_;
}

// ----------------------------------------------------------------------------

ScanCommand::ScanCommand(const std::vector<std::string>& /*params*/,
                         const std::map<std::string, std::string>& options,
                         const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, true,
          BuildCmdLineOptions({ARG_TTL, ARG_NO_VALUE, ARG_HEX, ARG_KEY_HEX,
                               ARG_TO, ARG_VALUE_HEX, ARG_FROM, ARG_TIMESTAMP,
                               ARG_MAX_KEYS, ARG_TTL_START, ARG_TTL_END})),
      start_key_specified_(false),
      end_key_specified_(false),
      max_keys_scanned_(-1),
      no_value_(false) {
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_FROM);
  if (itr != options.end()) {
    start_key_ = itr->second;
    if (is_key_hex_) {
      start_key_ = HexToString(start_key_);
    }
    start_key_specified_ = true;
  }
  itr = options.find(ARG_TO);
  if (itr != options.end()) {
    end_key_ = itr->second;
    if (is_key_hex_) {
      end_key_ = HexToString(end_key_);
    }
    end_key_specified_ = true;
  }

  std::vector<std::string>::const_iterator vitr =
      std::find(flags.begin(), flags.end(), ARG_NO_VALUE);
  if (vitr != flags.end()) {
    no_value_ = true;
  }

  itr = options.find(ARG_MAX_KEYS);
  if (itr != options.end()) {
    try {
#if defined(CYGWIN)
      max_keys_scanned_ = strtol(itr->second.c_str(), 0, 10);
#else
      max_keys_scanned_ = std::stoi(itr->second);
#endif
    } catch (const std::invalid_argument&) {
      exec_state_ = LDBCommandExecuteResult::Failed(ARG_MAX_KEYS +
                                                    " has an invalid value");
    } catch (const std::out_of_range&) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          ARG_MAX_KEYS + " has a value out-of-range");
    }
  }
}

void ScanCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ScanCommand::Name());
  ret.append(HelpRangeCmdArgs());
  ret.append(" [--" + ARG_TTL + "]");
  ret.append(" [--" + ARG_TIMESTAMP + "]");
  ret.append(" [--" + ARG_MAX_KEYS + "=<N>q] ");
  ret.append(" [--" + ARG_TTL_START + "=<N>:- is inclusive]");
  ret.append(" [--" + ARG_TTL_END + "=<N>:- is exclusive]");
  ret.append(" [--" + ARG_NO_VALUE + "]");
  ret.append("\n");
}

void ScanCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  int num_keys_scanned = 0;
  ReadOptions scan_read_opts;
  scan_read_opts.total_order_seek = true;
  Iterator* it = db_->NewIterator(scan_read_opts, GetCfHandle());
  if (start_key_specified_) {
    it->Seek(start_key_);
  } else {
    it->SeekToFirst();
  }
  int ttl_start;
  if (!ParseIntOption(option_map_, ARG_TTL_START, ttl_start, exec_state_)) {
    ttl_start = DBWithTTLImpl::kMinTimestamp;  // TTL introduction time
  }
  int ttl_end;
  if (!ParseIntOption(option_map_, ARG_TTL_END, ttl_end, exec_state_)) {
    ttl_end = DBWithTTLImpl::kMaxTimestamp;  // Max time allowed by TTL feature
  }
  if (ttl_end < ttl_start) {
    fprintf(stderr, "Error: End time can't be less than start time\n");
    delete it;
    return;
  }
  if (is_db_ttl_ && timestamp_) {
    fprintf(stdout, "Scanning key-values from %s to %s\n",
            TimeToHumanString(ttl_start).c_str(),
            TimeToHumanString(ttl_end).c_str());
  }
  for ( ;
        it->Valid() && (!end_key_specified_ || it->key().ToString() < end_key_);
        it->Next()) {
    if (is_db_ttl_) {
      TtlIterator* it_ttl = static_cast_with_check<TtlIterator>(it);
      int rawtime = it_ttl->ttl_timestamp();
      if (rawtime < ttl_start || rawtime >= ttl_end) {
        continue;
      }
      if (timestamp_) {
        fprintf(stdout, "%s ", TimeToHumanString(rawtime).c_str());
      }
    }

    Slice key_slice = it->key();

    std::string formatted_key;
    if (is_key_hex_) {
      formatted_key = "0x" + key_slice.ToString(true /* hex */);
      key_slice = formatted_key;
    } else if (ldb_options_.key_formatter) {
      formatted_key = ldb_options_.key_formatter->Format(key_slice);
      key_slice = formatted_key;
    }

    if (no_value_) {
      fprintf(stdout, "%.*s\n", static_cast<int>(key_slice.size()),
              key_slice.data());
    } else {
      Slice val_slice = it->value();
      std::string formatted_value;
      if (is_value_hex_) {
        formatted_value = "0x" + val_slice.ToString(true /* hex */);
        val_slice = formatted_value;
      }
      fprintf(stdout, "%.*s : %.*s\n", static_cast<int>(key_slice.size()),
              key_slice.data(), static_cast<int>(val_slice.size()),
              val_slice.data());
    }

    num_keys_scanned++;
    if (max_keys_scanned_ >= 0 && num_keys_scanned >= max_keys_scanned_) {
      break;
    }
  }
  if (!it->status().ok()) {  // Check for any errors found during the scan
    exec_state_ = LDBCommandExecuteResult::Failed(it->status().ToString());
  }
  delete it;
}

// ----------------------------------------------------------------------------

DeleteCommand::DeleteCommand(const std::vector<std::string>& params,
                             const std::map<std::string, std::string>& options,
                             const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX})) {
  if (params.size() != 1) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "KEY must be specified for the delete command");
  } else {
    key_ = params.at(0);
    if (is_key_hex_) {
      key_ = HexToString(key_);
    }
  }
}

void DeleteCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DeleteCommand::Name() + " <key>");
  ret.append("\n");
}

void DeleteCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Status st = db_->Delete(WriteOptions(), GetCfHandle(), key_);
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
  }
}

DeleteRangeCommand::DeleteRangeCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX})) {
  if (params.size() != 2) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "begin and end keys must be specified for the delete command");
  } else {
    begin_key_ = params.at(0);
    end_key_ = params.at(1);
    if (is_key_hex_) {
      begin_key_ = HexToString(begin_key_);
      end_key_ = HexToString(end_key_);
    }
  }
}

void DeleteRangeCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DeleteRangeCommand::Name() + " <begin key> <end key>");
  ret.append("\n");
}

void DeleteRangeCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Status st =
      db_->DeleteRange(WriteOptions(), GetCfHandle(), begin_key_, end_key_);
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
  }
}

PutCommand::PutCommand(const std::vector<std::string>& params,
                       const std::map<std::string, std::string>& options,
                       const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false,
                 BuildCmdLineOptions({ARG_TTL, ARG_HEX, ARG_KEY_HEX,
                                      ARG_VALUE_HEX, ARG_CREATE_IF_MISSING})) {
  if (params.size() != 2) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "<key> and <value> must be specified for the put command");
  } else {
    key_ = params.at(0);
    value_ = params.at(1);
  }

  if (is_key_hex_) {
    key_ = HexToString(key_);
  }

  if (is_value_hex_) {
    value_ = HexToString(value_);
  }
  create_if_missing_ = IsFlagPresent(flags_, ARG_CREATE_IF_MISSING);
}

void PutCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(PutCommand::Name());
  ret.append(" <key> <value>");
  ret.append(" [--" + ARG_CREATE_IF_MISSING + "]");
  ret.append(" [--" + ARG_TTL + "]");
  ret.append("\n");
}

void PutCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Status st = db_->Put(WriteOptions(), GetCfHandle(), key_, value_);
  if (st.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(st.ToString());
  }
}

void PutCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  options_.create_if_missing = create_if_missing_;
}

// ----------------------------------------------------------------------------

const char* DBQuerierCommand::HELP_CMD = "help";
const char* DBQuerierCommand::GET_CMD = "get";
const char* DBQuerierCommand::PUT_CMD = "put";
const char* DBQuerierCommand::DELETE_CMD = "delete";

DBQuerierCommand::DBQuerierCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, false,
          BuildCmdLineOptions({ARG_TTL, ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX})) {

}

void DBQuerierCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DBQuerierCommand::Name());
  ret.append(" [--" + ARG_TTL + "]");
  ret.append("\n");
  ret.append("    Starts a REPL shell.  Type help for list of available "
             "commands.");
  ret.append("\n");
}

void DBQuerierCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  ReadOptions read_options;
  WriteOptions write_options;

  std::string line;
  std::string key;
  std::string value;
  Status s;
  std::stringstream oss;
  while (s.ok() && getline(std::cin, line, '\n')) {
    // Parse line into std::vector<std::string>
    std::vector<std::string> tokens;
    size_t pos = 0;
    while (true) {
      size_t pos2 = line.find(' ', pos);
      if (pos2 == std::string::npos) {
        break;
      }
      tokens.push_back(line.substr(pos, pos2-pos));
      pos = pos2 + 1;
    }
    tokens.push_back(line.substr(pos));

    const std::string& cmd = tokens[0];

    if (cmd == HELP_CMD) {
      fprintf(stdout,
              "get <key>\n"
              "put <key> <value>\n"
              "delete <key>\n");
    } else if (cmd == DELETE_CMD && tokens.size() == 2) {
      key = (is_key_hex_ ? HexToString(tokens[1]) : tokens[1]);
      s = db_->Delete(write_options, GetCfHandle(), Slice(key));
      if (s.ok()) {
        fprintf(stdout, "Successfully deleted %s\n", tokens[1].c_str());
      } else {
        oss << "delete " << key << " failed: " << s.ToString();
      }
    } else if (cmd == PUT_CMD && tokens.size() == 3) {
      key = (is_key_hex_ ? HexToString(tokens[1]) : tokens[1]);
      value = (is_value_hex_ ? HexToString(tokens[2]) : tokens[2]);
      s = db_->Put(write_options, GetCfHandle(), Slice(key), Slice(value));
      if (s.ok()) {
        fprintf(stdout, "Successfully put %s %s\n", tokens[1].c_str(),
                tokens[2].c_str());
      } else {
        oss << "put " << key << "=>" << value << " failed: " << s.ToString();
      }
    } else if (cmd == GET_CMD && tokens.size() == 2) {
      key = (is_key_hex_ ? HexToString(tokens[1]) : tokens[1]);
      s = db_->Get(read_options, GetCfHandle(), Slice(key), &value);
      if (s.ok()) {
        fprintf(stdout, "%s\n", PrintKeyValue(key, value,
              is_key_hex_, is_value_hex_).c_str());
      } else {
        if (s.IsNotFound()) {
          fprintf(stdout, "Not found %s\n", tokens[1].c_str());
        } else {
          oss << "get " << key << " error: " << s.ToString();
        }
      }
    } else {
      fprintf(stdout, "Unknown command %s\n", line.c_str());
    }
  }
  if (!s.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(oss.str());
  }
}

// ----------------------------------------------------------------------------

CheckConsistencyCommand::CheckConsistencyCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, BuildCmdLineOptions({})) {}

void CheckConsistencyCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(CheckConsistencyCommand::Name());
  ret.append("\n");
}

void CheckConsistencyCommand::DoCommand() {
  options_.paranoid_checks = true;
  options_.num_levels = 64;
  OpenDB();
  if (exec_state_.IsSucceed() || exec_state_.IsNotStarted()) {
    fprintf(stdout, "OK\n");
  }
  CloseDB();
}

// ----------------------------------------------------------------------------

const std::string CheckPointCommand::ARG_CHECKPOINT_DIR = "checkpoint_dir";

CheckPointCommand::CheckPointCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false /* is_read_only */,
                 BuildCmdLineOptions({ARG_CHECKPOINT_DIR})) {
  auto itr = options.find(ARG_CHECKPOINT_DIR);
  if (itr != options.end()) {
    checkpoint_dir_ = itr->second;
  }
}

void CheckPointCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(CheckPointCommand::Name());
  ret.append(" [--" + ARG_CHECKPOINT_DIR + "] ");
  ret.append("\n");
}

void CheckPointCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Checkpoint* checkpoint;
  Status status = Checkpoint::Create(db_, &checkpoint);
  status = checkpoint->CreateCheckpoint(checkpoint_dir_);
  if (status.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(status.ToString());
  }
}

// ----------------------------------------------------------------------------

const std::string RepairCommand::ARG_VERBOSE = "verbose";

RepairCommand::RepairCommand(const std::vector<std::string>& /*params*/,
                             const std::map<std::string, std::string>& options,
                             const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false, BuildCmdLineOptions({ARG_VERBOSE})) {
  verbose_ = IsFlagPresent(flags, ARG_VERBOSE);
}

void RepairCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(RepairCommand::Name());
  ret.append(" [--" + ARG_VERBOSE + "]");
  ret.append("\n");
}

void RepairCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  auto level = verbose_ ? InfoLogLevel::INFO_LEVEL : InfoLogLevel::WARN_LEVEL;
  options_.info_log.reset(new StderrLogger(level));
}

void RepairCommand::DoCommand() {
  PrepareOptions();
  Status status = RepairDB(db_path_, options_);
  if (status.ok()) {
    fprintf(stdout, "OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(status.ToString());
  }
}

// ----------------------------------------------------------------------------

const std::string BackupableCommand::ARG_NUM_THREADS = "num_threads";
const std::string BackupableCommand::ARG_BACKUP_ENV_URI = "backup_env_uri";
const std::string BackupableCommand::ARG_BACKUP_FS_URI = "backup_fs_uri";
const std::string BackupableCommand::ARG_BACKUP_DIR = "backup_dir";
const std::string BackupableCommand::ARG_STDERR_LOG_LEVEL = "stderr_log_level";

BackupableCommand::BackupableCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false /* is_read_only */,
                 BuildCmdLineOptions({ARG_BACKUP_ENV_URI, ARG_BACKUP_FS_URI,
                                      ARG_BACKUP_DIR, ARG_NUM_THREADS,
                                      ARG_STDERR_LOG_LEVEL})),
      num_threads_(1) {
  auto itr = options.find(ARG_NUM_THREADS);
  if (itr != options.end()) {
    num_threads_ = std::stoi(itr->second);
  }
  itr = options.find(ARG_BACKUP_ENV_URI);
  if (itr != options.end()) {
    backup_env_uri_ = itr->second;
  }
  itr = options.find(ARG_BACKUP_FS_URI);
  if (itr != options.end()) {
    backup_fs_uri_ = itr->second;
  }
  if (!backup_env_uri_.empty() && !backup_fs_uri_.empty()) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "you may not specity both --" + ARG_BACKUP_ENV_URI + " and --" +
        ARG_BACKUP_FS_URI);
  }
  itr = options.find(ARG_BACKUP_DIR);
  if (itr == options.end()) {
    exec_state_ = LDBCommandExecuteResult::Failed("--" + ARG_BACKUP_DIR +
                                                  ": missing backup directory");
  } else {
    backup_dir_ = itr->second;
  }

  itr = options.find(ARG_STDERR_LOG_LEVEL);
  if (itr != options.end()) {
    int stderr_log_level = std::stoi(itr->second);
    if (stderr_log_level < 0 ||
        stderr_log_level >= InfoLogLevel::NUM_INFO_LOG_LEVELS) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          ARG_STDERR_LOG_LEVEL + " must be >= 0 and < " +
          std::to_string(InfoLogLevel::NUM_INFO_LOG_LEVELS) + ".");
    } else {
      logger_.reset(
          new StderrLogger(static_cast<InfoLogLevel>(stderr_log_level)));
    }
  }
}

void BackupableCommand::Help(const std::string& name, std::string& ret) {
  ret.append("  ");
  ret.append(name);
  ret.append(" [--" + ARG_BACKUP_ENV_URI + " | --" + ARG_BACKUP_FS_URI + "] ");
  ret.append(" [--" + ARG_BACKUP_DIR + "] ");
  ret.append(" [--" + ARG_NUM_THREADS + "] ");
  ret.append(" [--" + ARG_STDERR_LOG_LEVEL + "=<int (InfoLogLevel)>] ");
  ret.append("\n");
}

// ----------------------------------------------------------------------------

BackupCommand::BackupCommand(const std::vector<std::string>& params,
                             const std::map<std::string, std::string>& options,
                             const std::vector<std::string>& flags)
    : BackupableCommand(params, options, flags) {}

void BackupCommand::Help(std::string& ret) {
  BackupableCommand::Help(Name(), ret);
}

void BackupCommand::DoCommand() {
  BackupEngine* backup_engine;
  Status status;
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  fprintf(stdout, "open db OK\n");

  Env* custom_env = backup_env_guard_.get();
  if (custom_env == nullptr) {
    Status s =
        Env::CreateFromUri(config_options_, backup_env_uri_, backup_fs_uri_,
                           &custom_env, &backup_env_guard_);
    if (!s.ok()) {
      exec_state_ = LDBCommandExecuteResult::Failed(s.ToString());
      return;
    }
  }
  assert(custom_env != nullptr);

  BackupableDBOptions backup_options =
      BackupableDBOptions(backup_dir_, custom_env);
  backup_options.info_log = logger_.get();
  backup_options.max_background_operations = num_threads_;
  status = BackupEngine::Open(options_.env, backup_options, &backup_engine);
  if (status.ok()) {
    fprintf(stdout, "open backup engine OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(status.ToString());
    return;
  }
  status = backup_engine->CreateNewBackup(db_);
  if (status.ok()) {
    fprintf(stdout, "create new backup OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(status.ToString());
    return;
  }
}

// ----------------------------------------------------------------------------

RestoreCommand::RestoreCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : BackupableCommand(params, options, flags) {}

void RestoreCommand::Help(std::string& ret) {
  BackupableCommand::Help(Name(), ret);
}

void RestoreCommand::DoCommand() {
  Env* custom_env = backup_env_guard_.get();
  if (custom_env == nullptr) {
    Status s =
        Env::CreateFromUri(config_options_, backup_env_uri_, backup_fs_uri_,
                           &custom_env, &backup_env_guard_);
    if (!s.ok()) {
      exec_state_ = LDBCommandExecuteResult::Failed(s.ToString());
      return;
    }
  }
  assert(custom_env != nullptr);

  std::unique_ptr<BackupEngineReadOnly> restore_engine;
  Status status;
  {
    BackupableDBOptions opts(backup_dir_, custom_env);
    opts.info_log = logger_.get();
    opts.max_background_operations = num_threads_;
    BackupEngineReadOnly* raw_restore_engine_ptr;
    status =
        BackupEngineReadOnly::Open(options_.env, opts, &raw_restore_engine_ptr);
    if (status.ok()) {
      restore_engine.reset(raw_restore_engine_ptr);
    }
  }
  if (status.ok()) {
    fprintf(stdout, "open restore engine OK\n");
    status = restore_engine->RestoreDBFromLatestBackup(db_path_, db_path_);
  }
  if (status.ok()) {
    fprintf(stdout, "restore from backup OK\n");
  } else {
    exec_state_ = LDBCommandExecuteResult::Failed(status.ToString());
  }
}

// ----------------------------------------------------------------------------

namespace {

void DumpSstFile(Options options, std::string filename, bool output_hex,
                 bool show_properties) {
  std::string from_key;
  std::string to_key;
  if (filename.length() <= 4 ||
      filename.rfind(".sst") != filename.length() - 4) {
    std::cout << "Invalid sst file name." << std::endl;
    return;
  }
  // no verification
  // TODO: add support for decoding blob indexes in ldb as well
  ROCKSDB_NAMESPACE::SstFileDumper dumper(
      options, filename, 2 * 1024 * 1024 /* readahead_size */,
      /* verify_checksum */ false, output_hex,
      /* decode_blob_index */ false);
  Status st = dumper.ReadSequential(true, std::numeric_limits<uint64_t>::max(),
                                    false,            // has_from
                                    from_key, false,  // has_to
                                    to_key);
  if (!st.ok()) {
    std::cerr << "Error in reading SST file " << filename << st.ToString()
              << std::endl;
    return;
  }

  if (show_properties) {
    const ROCKSDB_NAMESPACE::TableProperties* table_properties;

    std::shared_ptr<const ROCKSDB_NAMESPACE::TableProperties>
        table_properties_from_reader;
    st = dumper.ReadTableProperties(&table_properties_from_reader);
    if (!st.ok()) {
      std::cerr << filename << ": " << st.ToString()
                << ". Try to use initial table properties" << std::endl;
      table_properties = dumper.GetInitTableProperties();
    } else {
      table_properties = table_properties_from_reader.get();
    }
    if (table_properties != nullptr) {
      std::cout << std::endl << "Table Properties:" << std::endl;
      std::cout << table_properties->ToString("\n") << std::endl;
    }
  }
}

}  // namespace

DBFileDumperCommand::DBFileDumperCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, BuildCmdLineOptions({})) {}

void DBFileDumperCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DBFileDumperCommand::Name());
  ret.append("\n");
}

void DBFileDumperCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Status s;

  std::cout << "Manifest File" << std::endl;
  std::cout << "==============================" << std::endl;
  std::string manifest_filename;
  s = ReadFileToString(db_->GetEnv(), CurrentFileName(db_->GetName()),
                       &manifest_filename);
  if (!s.ok() || manifest_filename.empty() ||
      manifest_filename.back() != '\n') {
    std::cerr << "Error when reading CURRENT file "
              << CurrentFileName(db_->GetName()) << std::endl;
  }
  // remove the trailing '\n'
  manifest_filename.resize(manifest_filename.size() - 1);
  std::string manifest_filepath = db_->GetName() + "/" + manifest_filename;
  // Correct concatenation of filepath and filename:
  // Check that there is no double slashes (or more!) when concatenation
  // happens.
  manifest_filepath = NormalizePath(manifest_filepath);

  std::cout << manifest_filepath << std::endl;
  DumpManifestFile(options_, manifest_filepath, false, false, false);
  std::cout << std::endl;

  std::cout << "SST Files" << std::endl;
  std::cout << "==============================" << std::endl;
  std::vector<LiveFileMetaData> metadata;
  db_->GetLiveFilesMetaData(&metadata);
  for (auto& fileMetadata : metadata) {
    std::string filename = fileMetadata.db_path + "/" + fileMetadata.name;
    // Correct concatenation of filepath and filename:
    // Check that there is no double slashes (or more!) when concatenation
    // happens.
    filename = NormalizePath(filename);
    std::cout << filename << " level:" << fileMetadata.level << std::endl;
    std::cout << "------------------------------" << std::endl;
    DumpSstFile(options_, filename, false, true);
    std::cout << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Write Ahead Log Files" << std::endl;
  std::cout << "==============================" << std::endl;
  ROCKSDB_NAMESPACE::VectorLogPtr wal_files;
  s = db_->GetSortedWalFiles(wal_files);
  if (!s.ok()) {
    std::cerr << "Error when getting WAL files" << std::endl;
  } else {
    std::string wal_dir;
    if (options_.wal_dir.empty()) {
      wal_dir = db_->GetName();
    } else {
      wal_dir = NormalizePath(options_.wal_dir + "/");
    }
    for (auto& wal : wal_files) {
      // TODO(qyang): option.wal_dir should be passed into ldb command
      std::string filename = wal_dir + wal->PathName();
      std::cout << filename << std::endl;
      // TODO(myabandeh): allow configuring is_write_commited
      DumpWalFile(options_, filename, true, true, true /* is_write_commited */,
                  &exec_state_);
    }
  }
}

const std::string DBLiveFilesMetadataDumperCommand::ARG_SORT_BY_FILENAME =
    "sort_by_filename";

DBLiveFilesMetadataDumperCommand::DBLiveFilesMetadataDumperCommand(
    const std::vector<std::string>& /*params*/,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true,
                 BuildCmdLineOptions({ARG_SORT_BY_FILENAME})) {
  sort_by_filename_ = IsFlagPresent(flags, ARG_SORT_BY_FILENAME);
}

void DBLiveFilesMetadataDumperCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(DBLiveFilesMetadataDumperCommand::Name());
  ret.append(" [--" + ARG_SORT_BY_FILENAME + "] ");
  ret.append("\n");
}

void DBLiveFilesMetadataDumperCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  Status s;

  std::vector<ColumnFamilyMetaData> metadata;
  db_->GetAllColumnFamilyMetaData(&metadata);
  if (sort_by_filename_) {
    std::cout << "Live SST and Blob Files:" << std::endl;
    // tuple of <file path, level, column family name>
    std::vector<std::tuple<std::string, int, std::string>> all_files;

    for (const auto& column_metadata : metadata) {
      // Iterate Levels
      const auto& levels = column_metadata.levels;
      const std::string& cf = column_metadata.name;
      for (const auto& level_metadata : levels) {
        // Iterate SST files
        const auto& sst_files = level_metadata.files;
        int level = level_metadata.level;
        for (const auto& sst_metadata : sst_files) {
          // The SstFileMetaData.name always starts with "/",
          // however SstFileMetaData.db_path is the string provided by
          // the user as an input. Therefore we check if we can
          // concantenate the two strings directly or if we need to
          // drop a possible extra "/" at the end of SstFileMetaData.db_path.
          std::string filename =
              NormalizePath(sst_metadata.db_path + "/" + sst_metadata.name);
          all_files.emplace_back(filename, level, cf);
        }  // End of for-loop over sst files
      }    // End of for-loop over levels

      const auto& blob_files = column_metadata.blob_files;
      for (const auto& blob_metadata : blob_files) {
        // The BlobMetaData.blob_file_name always starts with "/",
        // however BlobMetaData.blob_file_path is the string provided by
        // the user as an input. Therefore we check if we can
        // concantenate the two strings directly or if we need to
        // drop a possible extra "/" at the end of BlobMetaData.blob_file_path.
        std::string filename = NormalizePath(
            blob_metadata.blob_file_path + "/" + blob_metadata.blob_file_name);
        // Level for blob files is encoded as -1
        all_files.emplace_back(filename, -1, cf);
      }  // End of for-loop over blob files
    }    // End of for-loop over column metadata

    // Sort by filename (i.e. first entry in tuple)
    std::sort(all_files.begin(), all_files.end());

    for (const auto& item : all_files) {
      const std::string& filename = std::get<0>(item);
      int level = std::get<1>(item);
      const std::string& cf = std::get<2>(item);
      if (level == -1) {  // Blob File
        std::cout << filename << ", column family '" << cf << "'" << std::endl;
      } else {  // SST file
        std::cout << filename << " : level " << level << ", column family '"
                  << cf << "'" << std::endl;
      }
    }
  } else {
    for (const auto& column_metadata : metadata) {
      std::cout << "===== Column Family: " << column_metadata.name
                << " =====" << std::endl;

      std::cout << "Live SST Files:" << std::endl;
      // Iterate levels
      const auto& levels = column_metadata.levels;
      for (const auto& level_metadata : levels) {
        std::cout << "---------- level " << level_metadata.level
                  << " ----------" << std::endl;
        // Iterate SST files
        const auto& sst_files = level_metadata.files;
        for (const auto& sst_metadata : sst_files) {
          // The SstFileMetaData.name always starts with "/",
          // however SstFileMetaData.db_path is the string provided by
          // the user as an input. Therefore we check if we can
          // concantenate the two strings directly or if we need to
          // drop a possible extra "/" at the end of SstFileMetaData.db_path.
          std::string filename =
              NormalizePath(sst_metadata.db_path + "/" + sst_metadata.name);
          std::cout << filename << std::endl;
        }  // End of for-loop over sst files
      }    // End of for-loop over levels

      std::cout << "Live Blob Files:" << std::endl;
      const auto& blob_files = column_metadata.blob_files;
      for (const auto& blob_metadata : blob_files) {
        // The BlobMetaData.blob_file_name always starts with "/",
        // however BlobMetaData.blob_file_path is the string provided by
        // the user as an input. Therefore we check if we can
        // concantenate the two strings directly or if we need to
        // drop a possible extra "/" at the end of BlobMetaData.blob_file_path.
        std::string filename = NormalizePath(
            blob_metadata.blob_file_path + "/" + blob_metadata.blob_file_name);
        std::cout << filename << std::endl;
      }  // End of for-loop over blob files
    }    // End of for-loop over column metadata
  }      // End of else ("not sort_by_filename")
  std::cout << "------------------------------" << std::endl;
}

void WriteExternalSstFilesCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(WriteExternalSstFilesCommand::Name());
  ret.append(" <output_sst_path>");
  ret.append("\n");
}

WriteExternalSstFilesCommand::WriteExternalSstFilesCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, false /* is_read_only */,
          BuildCmdLineOptions({ARG_HEX, ARG_KEY_HEX, ARG_VALUE_HEX, ARG_FROM,
                               ARG_TO, ARG_CREATE_IF_MISSING})) {
  create_if_missing_ =
      IsFlagPresent(flags, ARG_CREATE_IF_MISSING) ||
      ParseBooleanOption(options, ARG_CREATE_IF_MISSING, false);
  if (params.size() != 1) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "output SST file path must be specified");
  } else {
    output_sst_path_ = params.at(0);
  }
}

void WriteExternalSstFilesCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  ColumnFamilyHandle* cfh = GetCfHandle();
  SstFileWriter sst_file_writer(EnvOptions(), db_->GetOptions(), cfh);
  Status status = sst_file_writer.Open(output_sst_path_);
  if (!status.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed("failed to open SST file: " +
                                                  status.ToString());
    return;
  }

  int bad_lines = 0;
  std::string line;
  std::ifstream ifs_stdin("/dev/stdin");
  std::istream* istream_p = ifs_stdin.is_open() ? &ifs_stdin : &std::cin;
  while (getline(*istream_p, line, '\n')) {
    std::string key;
    std::string value;
    if (ParseKeyValue(line, &key, &value, is_key_hex_, is_value_hex_)) {
      status = sst_file_writer.Put(key, value);
      if (!status.ok()) {
        exec_state_ = LDBCommandExecuteResult::Failed(
            "failed to write record to file: " + status.ToString());
        return;
      }
    } else if (0 == line.find("Keys in range:")) {
      // ignore this line
    } else if (0 == line.find("Created bg thread 0x")) {
      // ignore this line
    } else {
      bad_lines++;
    }
  }

  status = sst_file_writer.Finish();
  if (!status.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "Failed to finish writing to file: " + status.ToString());
    return;
  }

  if (bad_lines > 0) {
    fprintf(stderr, "Warning: %d bad lines ignored.\n", bad_lines);
  }
  exec_state_ = LDBCommandExecuteResult::Succeed(
      "external SST file written to " + output_sst_path_);
}

void WriteExternalSstFilesCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  options_.create_if_missing = create_if_missing_;
}

const std::string IngestExternalSstFilesCommand::ARG_MOVE_FILES = "move_files";
const std::string IngestExternalSstFilesCommand::ARG_SNAPSHOT_CONSISTENCY =
    "snapshot_consistency";
const std::string IngestExternalSstFilesCommand::ARG_ALLOW_GLOBAL_SEQNO =
    "allow_global_seqno";
const std::string IngestExternalSstFilesCommand::ARG_ALLOW_BLOCKING_FLUSH =
    "allow_blocking_flush";
const std::string IngestExternalSstFilesCommand::ARG_INGEST_BEHIND =
    "ingest_behind";
const std::string IngestExternalSstFilesCommand::ARG_WRITE_GLOBAL_SEQNO =
    "write_global_seqno";

void IngestExternalSstFilesCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(IngestExternalSstFilesCommand::Name());
  ret.append(" <input_sst_path>");
  ret.append(" [--" + ARG_MOVE_FILES + "] ");
  ret.append(" [--" + ARG_SNAPSHOT_CONSISTENCY + "] ");
  ret.append(" [--" + ARG_ALLOW_GLOBAL_SEQNO + "] ");
  ret.append(" [--" + ARG_ALLOW_BLOCKING_FLUSH + "] ");
  ret.append(" [--" + ARG_INGEST_BEHIND + "] ");
  ret.append(" [--" + ARG_WRITE_GLOBAL_SEQNO + "] ");
  ret.append("\n");
}

IngestExternalSstFilesCommand::IngestExternalSstFilesCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(
          options, flags, false /* is_read_only */,
          BuildCmdLineOptions({ARG_MOVE_FILES, ARG_SNAPSHOT_CONSISTENCY,
                               ARG_ALLOW_GLOBAL_SEQNO, ARG_CREATE_IF_MISSING,
                               ARG_ALLOW_BLOCKING_FLUSH, ARG_INGEST_BEHIND,
                               ARG_WRITE_GLOBAL_SEQNO})),
      move_files_(false),
      snapshot_consistency_(true),
      allow_global_seqno_(true),
      allow_blocking_flush_(true),
      ingest_behind_(false),
      write_global_seqno_(true) {
  create_if_missing_ =
      IsFlagPresent(flags, ARG_CREATE_IF_MISSING) ||
      ParseBooleanOption(options, ARG_CREATE_IF_MISSING, false);
  move_files_ = IsFlagPresent(flags, ARG_MOVE_FILES) ||
                ParseBooleanOption(options, ARG_MOVE_FILES, false);
  snapshot_consistency_ =
      IsFlagPresent(flags, ARG_SNAPSHOT_CONSISTENCY) ||
      ParseBooleanOption(options, ARG_SNAPSHOT_CONSISTENCY, true);
  allow_global_seqno_ =
      IsFlagPresent(flags, ARG_ALLOW_GLOBAL_SEQNO) ||
      ParseBooleanOption(options, ARG_ALLOW_GLOBAL_SEQNO, true);
  allow_blocking_flush_ =
      IsFlagPresent(flags, ARG_ALLOW_BLOCKING_FLUSH) ||
      ParseBooleanOption(options, ARG_ALLOW_BLOCKING_FLUSH, true);
  ingest_behind_ = IsFlagPresent(flags, ARG_INGEST_BEHIND) ||
                   ParseBooleanOption(options, ARG_INGEST_BEHIND, false);
  write_global_seqno_ =
      IsFlagPresent(flags, ARG_WRITE_GLOBAL_SEQNO) ||
      ParseBooleanOption(options, ARG_WRITE_GLOBAL_SEQNO, true);

  if (allow_global_seqno_) {
    if (!write_global_seqno_) {
      fprintf(stderr,
              "Warning: not writing global_seqno to the ingested SST can\n"
              "prevent older versions of RocksDB from being able to open it\n");
    }
  } else {
    if (write_global_seqno_) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "ldb cannot write global_seqno to the ingested SST when global_seqno "
          "is not allowed");
    }
  }

  if (params.size() != 1) {
    exec_state_ =
        LDBCommandExecuteResult::Failed("input SST path must be specified");
  } else {
    input_sst_path_ = params.at(0);
  }
}

void IngestExternalSstFilesCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }
  if (GetExecuteState().IsFailed()) {
    return;
  }
  ColumnFamilyHandle* cfh = GetCfHandle();
  IngestExternalFileOptions ifo;
  ifo.move_files = move_files_;
  ifo.snapshot_consistency = snapshot_consistency_;
  ifo.allow_global_seqno = allow_global_seqno_;
  ifo.allow_blocking_flush = allow_blocking_flush_;
  ifo.ingest_behind = ingest_behind_;
  ifo.write_global_seqno = write_global_seqno_;
  Status status = db_->IngestExternalFile(cfh, {input_sst_path_}, ifo);
  if (!status.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "failed to ingest external SST: " + status.ToString());
  } else {
    exec_state_ =
        LDBCommandExecuteResult::Succeed("external SST files ingested");
  }
}

void IngestExternalSstFilesCommand::OverrideBaseOptions() {
  LDBCommand::OverrideBaseOptions();
  options_.create_if_missing = create_if_missing_;
}

ListFileRangeDeletesCommand::ListFileRangeDeletesCommand(
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, true, BuildCmdLineOptions({ARG_MAX_KEYS})) {
  std::map<std::string, std::string>::const_iterator itr =
      options.find(ARG_MAX_KEYS);
  if (itr != options.end()) {
    try {
#if defined(CYGWIN)
      max_keys_ = strtol(itr->second.c_str(), 0, 10);
#else
      max_keys_ = std::stoi(itr->second);
#endif
    } catch (const std::invalid_argument&) {
      exec_state_ = LDBCommandExecuteResult::Failed(ARG_MAX_KEYS +
                                                    " has an invalid value");
    } catch (const std::out_of_range&) {
      exec_state_ = LDBCommandExecuteResult::Failed(
          ARG_MAX_KEYS + " has a value out-of-range");
    }
  }
}

void ListFileRangeDeletesCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(ListFileRangeDeletesCommand::Name());
  ret.append(" [--" + ARG_MAX_KEYS + "=<N>]");
  ret.append(" : print tombstones in SST files.\n");
}

void ListFileRangeDeletesCommand::DoCommand() {
  if (!db_) {
    assert(GetExecuteState().IsFailed());
    return;
  }

  DBImpl* db_impl = static_cast_with_check<DBImpl>(db_->GetRootDB());

  std::string out_str;

  Status st =
      db_impl->TablesRangeTombstoneSummary(GetCfHandle(), max_keys_, &out_str);
  if (st.ok()) {
    TEST_SYNC_POINT_CALLBACK(
        "ListFileRangeDeletesCommand::DoCommand:BeforePrint", &out_str);
    fprintf(stdout, "%s\n", out_str.c_str());
  }
}

void UnsafeRemoveSstFileCommand::Help(std::string& ret) {
  ret.append("  ");
  ret.append(UnsafeRemoveSstFileCommand::Name());
  ret.append(" <SST file number>");
  ret.append("  ");
  ret.append("    MUST NOT be used on a live DB.");
  ret.append("\n");
}

UnsafeRemoveSstFileCommand::UnsafeRemoveSstFileCommand(
    const std::vector<std::string>& params,
    const std::map<std::string, std::string>& options,
    const std::vector<std::string>& flags)
    : LDBCommand(options, flags, false /* is_read_only */,
                 BuildCmdLineOptions({})) {
  if (params.size() != 1) {
    exec_state_ =
        LDBCommandExecuteResult::Failed("SST file number must be specified");
  } else {
    char* endptr = nullptr;
    sst_file_number_ = strtoull(params.at(0).c_str(), &endptr, 10 /* base */);
    if (endptr == nullptr || *endptr != '\0') {
      exec_state_ = LDBCommandExecuteResult::Failed(
          "Failed to parse SST file number " + params.at(0));
    }
  }
}

void UnsafeRemoveSstFileCommand::DoCommand() {
  // Instead of opening a `DB` and calling `DeleteFile()`, this implementation
  // uses the underlying `VersionSet` API to read and modify the MANIFEST. This
  // allows us to use the user's real options, while not having to worry about
  // the DB persisting new SST files via flush/compaction or attempting to read/
  // compact files which may fail, particularly for the file we intend to remove
  // (the user may want to remove an already deleted file from MANIFEST).
  PrepareOptions();

  if (options_.db_paths.empty()) {
    // `VersionSet` expects options that have been through `SanitizeOptions()`,
    // which would sanitize an empty `db_paths`.
    options_.db_paths.emplace_back(db_path_, 0 /* target_size */);
  }

  WriteController wc(options_.delayed_write_rate);
  WriteBufferManager wb(options_.db_write_buffer_size);
  ImmutableDBOptions immutable_db_options(options_);
  std::shared_ptr<Cache> tc(
      NewLRUCache(1 << 20 /* capacity */, options_.table_cache_numshardbits));
  EnvOptions sopt;
  VersionSet versions(db_path_, &immutable_db_options, sopt, tc.get(), &wb, &wc,
                      /*block_cache_tracer=*/nullptr, /*io_tracer=*/nullptr,
                      /*db_session_id*/ "");
  Status s = versions.Recover(column_families_);

  ColumnFamilyData* cfd = nullptr;
  int level = -1;
  if (s.ok()) {
    FileMetaData* metadata = nullptr;
    s = versions.GetMetadataForFile(sst_file_number_, &level, &metadata, &cfd);
  }

  if (s.ok()) {
    VersionEdit edit;
    edit.SetColumnFamily(cfd->GetID());
    edit.DeleteFile(level, sst_file_number_);
    // Use `mutex` to imitate a locked DB mutex when calling `LogAndApply()`.
    InstrumentedMutex mutex;
    mutex.Lock();
    s = versions.LogAndApply(cfd, *cfd->GetLatestMutableCFOptions(), &edit,
                             &mutex, nullptr /* db_directory */,
                             false /* new_descriptor_log */);
    mutex.Unlock();
  }

  if (!s.ok()) {
    exec_state_ = LDBCommandExecuteResult::Failed(
        "failed to unsafely remove SST file: " + s.ToString());
  } else {
    exec_state_ = LDBCommandExecuteResult::Succeed("unsafely removed SST file");
  }
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
