////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
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
#include "common.hpp"
#include "index-dump.hpp"
#include "index/directory_reader.hpp"
#include "index/field_meta.hpp"
#include "analysis/token_attributes.hpp"

#include <fstream>
#include <iostream>

namespace {

const std::string HELP = "help";
const std::string INDEX_DIR = "index-dir";
const std::string DIR_TYPE = "dir-type";
const std::string OUTPUT = "out";

}

int dump(
    const std::string& path,
    const std::string& dir_type,
    std::ostream& stream) {
  auto dir = create_directory(dir_type, path);

  if (!dir) {
    std::cerr << "Unable to create directory of type '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto reader = irs::directory_reader::open(*dir, irs::formats::get("1_0"));

  stream << "Index" 
         << " segmentsCount=" << reader.size()
         << " docsCount=" << reader.docs_count()
         << " liveDocsCount=" << reader.live_docs_count() << std::endl;

  size_t i = 0;
  for (auto& segment : reader) {
    stream << "Segment id=" << i 
           << " docsCount=" << segment.docs_count()
           << " liveDocsCount=" << segment.live_docs_count() << std::endl;

    for (auto fields = segment.fields();fields->next();) {
      auto& field = fields->value();
      auto& meta = field.meta();
      stream << "Field name=" << meta.name
             << " norm=" << meta.norm
             << " minTerm=" << irs::ref_cast<char>(field.min()) 
             << " maxTerm=" << irs::ref_cast<char>(field.max())
             << " termsCount=" << field.size()
             << " docsCount=" << field.docs_count()
             << std::endl;

      auto term = field.iterator();
      auto const& term_meta = irs::get<irs::term_meta>(*term);
      stream << "Values" << std::endl;
      for (; term->next(); ) {
        term->read();
        stream << "Term value=" << irs::ref_cast<char>(term->value()) << ""
               << " docsCount=" << term_meta->docs_count
               << std::endl;
      }
    }

    for (auto columns = segment.columns(); columns->next();) {
      auto& meta = columns->value();
      stream << "Column id=" << meta.id
             << " name=" << meta.name
             << std::endl;

      auto visitor = [&stream](irs::doc_id_t doc, const irs::bytes_ref& value) {
        stream << "doc=" << doc
               << " value=" << irs::ref_cast<char>(value)
               << std::endl;
        return true;
      };

      auto column = segment.column_reader(meta.id);
      column->visit(visitor);
    }
    ++i;
  }

  return 0;
}

int dump(const cmdline::parser& args) {
  if (!args.exist(INDEX_DIR)) {
    return 1;
  }

  const auto& path = args.get<std::string>(INDEX_DIR);

  if (path.empty()) {
    return 1;
  }

  const auto dir_type = args.exist(DIR_TYPE)
    ? args.get<std::string>(DIR_TYPE) 
    : std::string("fs");

  if (args.exist(OUTPUT)) {
    const auto& file = args.get<std::string>(OUTPUT);
    std::fstream out(file, std::fstream::out | std::fstream::trunc);
    if (!out) {
      return 1;
    }

    return dump(path, dir_type, out);
  }

  return dump(path, dir_type, std::cout);
}

int dump(int argc, char* argv[]) {
  // dump mode description
  cmdline::parser cmddump;
  cmddump.add(HELP, '?', "Produce help message");
  cmddump.add<std::string>(INDEX_DIR, 0, "Path to index directory", true);
  cmddump.add<std::string>(DIR_TYPE, 0, "Directory type (fs|mmap)", false, std::string("fs"));
  cmddump.add<std::string>(OUTPUT, 0, "Output file", false);

  cmddump.parse(argc, argv);

  if (cmddump.exist(HELP)) {
    std::cout << cmddump.usage() << std::endl;
    return 0;
  }

  return dump(cmddump);
}
