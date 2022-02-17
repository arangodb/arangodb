////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <functional>
#include <iomanip>

#if defined(_MSC_VER)
  #pragma warning(disable: 4101)
  #pragma warning(disable: 4267)
#endif

#include <cmdline.h>

#if defined(_MSC_VER)
  #pragma warning(default: 4267)
  #pragma warning(default: 4101)
#endif

#include "shared.hpp"
#include "store/store_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/compression.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                               handle registration
// -----------------------------------------------------------------------------

typedef std::unordered_map<
  std::string,
  std::function<int(int argc, char* argv[])>
> handlers_t;

int dump(int argc, char* argv[]);

const std::string MODE_DUMP = "dump";

bool init_handlers(handlers_t& handlers) {
  handlers.emplace(MODE_DUMP, &dump);
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               dump implementation
// -----------------------------------------------------------------------------

const std::string HELP = "help";
const std::string DISTANCE = "distance";
const std::string LINE_LENGTH = "line-length";
const std::string TRANSPOSITIONS = "transpositions";
const std::string COMPRESSION = "compression";
const std::string NAMEPSACE = "namespace";

int dump(
    irs::byte_type distance,
    bool with_transpositions,
    size_t line_length,
    irs::compression::compressor::ptr comp,
    const std::string& ns) {
  auto& out = std::cout;

  const auto d = irs::make_parametric_description(distance, with_transpositions);

  if (!d) {
    return 1;
  }

  irs::bstring raw;

  // write description to string
  {
    irs::bytes_output out(raw);
    irs::write(d, static_cast<data_output&>(out));
  }

  irs::bstring buf;
  irs::bytes_ref compressed;
  if (comp) {
    compressed = comp->compress(&raw[0], raw.size(), buf);
  } else {
    compressed = raw;
  }

  if (!ns.empty()) {
    out << "namespace " << ns << "{\n";
  }

  out << "constexpr size_t PDD_RAW_LEN = " << raw.size() << ";\n";
  out << "constexpr size_t PDD_COMPRESSED_LEN = " << compressed.size() << ";\n";
  out << "constexpr unsigned char PDD[] = {";

  out << std::internal
      << std::setfill('0')
      << std::hex;

  for (auto begin = compressed.begin(), end = compressed.end(); begin != end;) {
    out << "\n ";
    auto slice_begin = begin;
    auto slice_end = std::min(begin + line_length, compressed.end());
    for (; slice_begin != slice_end; ++slice_begin) {
      out << " 0x" << std::setw(2) << uint32_t(*slice_begin) << ",";
    }
    begin = slice_end;
  }
  out << std::dec;
  out << "\n};";

  if (!ns.empty()) {
    out << "\n}";
  }

  return 0;
}

int dump(const cmdline::parser& args) {
  if (!args.exist(DISTANCE)) {
    return 1;
  }

  const size_t distance = args.get<size_t>(DISTANCE);
  const size_t items_per_line = args.get<size_t>(LINE_LENGTH);
  const bool with_transpositions = args.get<bool>(TRANSPOSITIONS);
  const std::string compression = args.get<std::string>(COMPRESSION);
  const std::string ns = args.get<std::string>(NAMEPSACE);

  if (distance > irs::parametric_description::MAX_DISTANCE) {
    return 1;
  }

  irs::compression::init(); // load built-in compressors

  if (!irs::compression::exists(compression)) {
    return 1;
  }

  const irs::compression::options opts{ irs::compression::options::Hint::COMPRESSION };
  auto compressor = irs::compression::get_compressor(compression, opts);

  return dump(static_cast<irs::byte_type>(distance), with_transpositions, items_per_line, compressor, ns);
}

int dump(int argc, char* argv[]) {
  cmdline::parser cmd;
  cmd.add(HELP, '?', "Produce help message");
  cmd.add<size_t>(DISTANCE, 'd', "Maximum edit distance", true, size_t(1));
  cmd.add<size_t>(LINE_LENGTH, 0, "Items per line", false, size_t(16));
  cmd.add<bool>(TRANSPOSITIONS, 't', "Count transpositions", false, false);
  cmd.add<std::string>(COMPRESSION, 0, "compression", false, "iresearch::compression::lz4");
  cmd.add<std::string>(NAMEPSACE, 'n', "namespace", false, "");

  cmd.parse(argc, argv);

  if (cmd.exist(HELP)) {
    std::cout << cmd.usage() << std::endl;
    return 0;
  }

  return dump(cmd);
}
