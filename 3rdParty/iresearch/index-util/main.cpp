//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "index/directory_reader.hpp"
#include "index/field_meta.hpp"
#include "store/fs_directory.hpp"
#include "analysis/token_attributes.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>

namespace {

namespace po = boost::program_options;
namespace fs = boost::filesystem;

}

static const std::string INDEX_DIR = "index-dir";
static const std::string OUTPUT = "out";

int dump(const std::string& path, std::ostream& stream) {
  irs::fs_directory dir(path);
  auto reader = irs::directory_reader::open(dir, irs::formats::get("1_0"));

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
      auto& term_meta = term->attributes().get<irs::term_meta>();
      stream << "Values" << std::endl;
      for (; term->next(); ) {
        term->read();
        stream << "Term value=" << irs::ref_cast<char>(term->value()) << ""
               << " docsCount=" << term_meta->docs_count
               << std::endl;
      }
    }
    ++i;
  }
  return 0;
}

int dump(const po::variables_map& vm) {
  if (!vm.count(INDEX_DIR)) {
    return 1;
  }

  auto& path = vm[INDEX_DIR].as<std::string>();

  if (path.empty()) {
    return 1;
  }

  if (vm.count(OUTPUT)) {
    auto& file = vm[OUTPUT].as<std::string>();
    std::fstream out(file, std::fstream::out | std::fstream::trunc);
    if (!out) {
      return 1;
    }

    return dump(path, out);
  }

  return dump(path, std::cout);
}

int main(int argc, char* argv[]) {
  po::variables_map vm;
  
  // general description
  std::string mode;
  po::options_description desc("\n[IReSearch-index-util] General options");
  desc.add_options()
    ("help,h", "produce help message")
    ("mode,m", po::value<std::string>(&mode), "Select mode: dump");

  // stats mode description
  po::options_description stats_desc("Dump mode options");
  stats_desc.add_options()
    (INDEX_DIR.c_str(), po::value<std::string>(), "Path to index directory")
    (OUTPUT.c_str(), po::value<std::string>(), "Output file");

  po::command_line_parser parser(argc, argv);
  parser.options(desc).allow_unregistered();
  po::parsed_options options = parser.run();
  po::store(options, vm);
  po::notify(vm);

  // show help
  if (vm.count("help")) {
    desc.add(stats_desc);
    std::cout << desc << std::endl;
    return 0;
  }

  // enter dump mode
  if ("dump" == mode) {
    desc.add(stats_desc);
    po::store(po::parse_command_line(argc, argv, desc), vm);

    return dump(vm);
  }

  return 0;
}
