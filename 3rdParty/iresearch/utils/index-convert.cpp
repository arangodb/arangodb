////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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
#include "index-convert.hpp"
#include "common.hpp"

#include "formats/formats.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"

namespace {

const std::string HELP = "help";
const std::string DIR_TYPE = "dir-type";
const std::string IN_DIR = "in";
const std::string OUT_DIR = "out";
const std::string OUT_FORMAT = "out-format";
const std::string FORMATS_DIR = "format-dir";

}

int convert(
    const std::string& in,
    const std::string& out,
    const std::string& dir_type,
    const std::string& format) {
  auto in_dir = create_directory(dir_type, in);

  if (!in_dir) {
    std::cerr << "Unable to create input directory directory of type '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto out_dir = create_directory(dir_type, out);

  if (!out_dir) {
    std::cerr << "Unable to create input directory directory of type '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto codec = irs::formats::get(format);

  if (!codec) {
    std::cerr << "Unable to load the specified format '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto reader = irs::directory_reader::open(*in_dir);
  auto writer =
    irs::index_writer::make(*out_dir, codec, irs::OM_CREATE | irs::OM_APPEND);

  writer->import(*reader);
  writer->commit();

  return 0;
}

int convert(const cmdline::parser& args) {
  if (!args.exist(IN_DIR)
      || !args.exist(OUT_DIR)
      || !args.exist(OUT_FORMAT)) {
    return 1;
  }

  const auto& in_path = args.get<std::string>(IN_DIR);

  if (in_path.empty()) {
    return 1;
  }

  const auto& out_path = args.get<std::string>(OUT_DIR);

  if (out_path.empty()) {
    return 1;
  }

  const auto& out_format = args.get<std::string>(OUT_FORMAT);

  const auto dir_type = args.exist(DIR_TYPE)
    ? args.get<std::string>(DIR_TYPE) 
    : std::string("fs");

  // init formats
  irs::formats::init();
  if (args.exist(FORMATS_DIR)) {
    auto& formats_dir = args.get<std::string>(FORMATS_DIR);
    if (!formats_dir.empty()) {
      irs::formats::load_all(formats_dir);
    }
  }

  return convert(in_path, out_path, dir_type, out_format);
}

int convert(int argc, char* argv[]) {
  // convert mode description
  cmdline::parser cmdconv;
  cmdconv.add(HELP, '?', "Produce help message");
  cmdconv.add<std::string>(IN_DIR, 0, "Path to input index directory", true);
  cmdconv.add<std::string>(OUT_DIR, 0, "Path to output index directory", true);
  cmdconv.add<std::string>(FORMATS_DIR, 0, "Plugin directory", false);
  cmdconv.add<std::string>(OUT_FORMAT, 0, "Output format", true);
  cmdconv.add<std::string>(DIR_TYPE, 0, "Directory type (fs|mmap)", false, std::string("fs"));

  cmdconv.parse(argc, argv);

  if (cmdconv.exist(HELP)) {
    std::cout << cmdconv.usage() << std::endl;
    return 0;
  }

  return convert(cmdconv);
}
