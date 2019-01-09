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

#if defined(__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <boost/thread.hpp>

#if defined(__GNUC__)
  #pragma GCC diagnostic pop
#endif

#if defined(_MSC_VER)
  #pragma warning(disable: 4101)
  #pragma warning(disable: 4267)
#endif

  #include <cmdline.h>

#if defined(_MSC_VER)
  #pragma warning(default: 4267)
  #pragma warning(default: 4101)
#endif

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

  #include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

#include "index-put.hpp"
#include "common.hpp"
#include "index/index_writer.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/text_token_stream.hpp"
#include "store/store_utils.hpp"
#include "utils/index_utils.hpp"

#include <boost/chrono.hpp>
#include <fstream>
#include <iostream>

#if defined(_MSC_VER) && (_MSC_VER < 1900)
  #define snprintf _snprintf
#endif

NS_LOCAL

const std::string HELP = "help";
const std::string BATCH_SIZE = "batch-size";
const std::string CONSOLIDATE = "consolidate";
const std::string INDEX_DIR = "index-dir";
const std::string OUTPUT = "out";
const std::string INPUT = "in";
const std::string MAX = "max-lines";
const std::string THR = "threads";
const std::string CPR = "commit-period";
const std::string DIR_TYPE = "dir-type";
const std::string FORMAT = "format";

typedef std::unique_ptr<std::string> ustringp;

const std::string n_id = "id";
const std::string n_title = "title";
const std::string n_date = "date";
const std::string n_timesecnum = "timesecnum";
const std::string n_body = "body";

const irs::flags text_features{
  irs::frequency::type(),
  irs::position::type(),
  irs::offset::type(),
  irs::norm::type()
};

const irs::flags numeric_features{
  irs::granularity_prefix::type()
};

NS_END

struct Doc {
  static std::atomic<uint64_t> next_id;

  /**
   * C++ version 0.4 char* style "itoa":
   * Written by Lukás Chmela
   * Released under GPLv3.
   */
  char* itoa(int value, char* result, int base) {
    // check that the base if valid
    if (base < 2 || base > 36) {
      *result = '\0';
      return result;
    }

    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;

    do {
      tmp_value = value;
      value /= base;
      *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while (value);

    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while (ptr1 < ptr) {
      tmp_char = *ptr;
      *ptr-- = *ptr1;
      *ptr1++ = tmp_char;
    }
    return result;
  }

  /**
  */
  struct Field {
    const std::string& _name;
    const irs::flags feats;

    Field(const std::string& n, const irs::flags& flags) 
      : _name(n), feats(flags) {
    }

    const std::string& name() const {
      return _name;
    }

    float_t boost() const {
      return 1.0;
    }

    virtual irs::token_stream& get_tokens() const = 0;

    const irs::flags& features() const {
      return feats;
    }

    virtual bool write(irs::data_output& out) const = 0;

    virtual ~Field() {}
  };

  struct StringField : public Field {
    std::string f;
    mutable irs::string_token_stream _stream;

    StringField(const std::string& n, const irs::flags& flags)
      : Field(n, flags) {
    }

    StringField(const std::string& n, const irs::flags& flags, const std::string& a)
      : Field(n, flags), f(a) {
    }

    irs::token_stream& get_tokens() const override {
      _stream.reset(f);
      return _stream;
    }

    bool write(irs::data_output& out) const override {
      irs::write_string(out, f.c_str(), f.length());
      return true;
    }
  };

  struct TextField : public Field {
    std::string f;
    mutable irs::analysis::analyzer::ptr stream;
    static const std::string& aname;
    static const std::string& aignore;
    static const irs::text_format::type_id& aignore_format;

    TextField(const std::string& n, const irs::flags& flags)
      : Field(n, flags) {
      stream = irs::analysis::analyzers::get(aname, aignore_format, aignore);
    }

    TextField(const std::string& n, const irs::flags& flags, std::string& a)
      : Field(n, flags), f(a) {
      stream = irs::analysis::analyzers::get(aname, aignore_format, aignore);
    }

    irs::token_stream& get_tokens() const override {
      stream->reset(f);
      return *stream;
    }

    bool write(irs::data_output& out) const override {
      irs::write_string(out, f.c_str(), f.length());
      return true;
    }
  };

  struct NumericField : public Field {
    mutable irs::numeric_token_stream stream;
    int64_t value;

    NumericField(const std::string& n, const irs::flags& flags)
      : Field(n, flags) {
    }

    NumericField(const std::string& n, const irs::flags& flags, uint64_t v)
      : Field(n, flags), value(v) {
    }

    irs::token_stream& get_tokens() const override {
      stream.reset(value);
      return stream;
    }

    bool write(irs::data_output& out) const override {
      irs::write_zvlong(out, value);
      return true;
    }
  };

  std::vector<std::shared_ptr<Field>> elements;
  std::vector<std::shared_ptr<Field>> store;

  /**
   * Parse line to fields
   * @todo way too many string copies here
   * @param line
   * @return 
   */
  virtual void fill(std::string* line) {
    std::stringstream lineStream(*line);
    std::string cell;

    // id: uint64_t to string, base 36
    uint64_t id = next_id++; // atomic fetch and get
    char str[10];
    itoa(id, str, 36);
    char str2[10];
    snprintf(str2, sizeof (str2), "%6s", str);
    std::string s(str2);
    std::replace(s.begin(), s.end(), ' ', '0');
    elements.emplace_back(std::make_shared<StringField>(n_id, irs::flags{irs::granularity_prefix::type()}, s));
    store.emplace_back(elements.back());

    // title: string
    std::getline(lineStream, cell, '\t');
    elements.emplace_back(std::make_shared<StringField>(n_title, irs::flags::empty_instance(), cell));

    // date: string
    std::getline(lineStream, cell, '\t');
    elements.emplace_back(std::make_shared<StringField>(n_date, irs::flags::empty_instance(), cell));
    store.emplace_back(elements.back());

    // +date: uint64_t
    uint64_t t = 0; //boost::posix_time::microsec_clock::local_time().total_milliseconds();
    elements.emplace_back(
      std::make_shared<NumericField>(n_timesecnum, irs::flags{irs::granularity_prefix::type()}, t)
    );

    // body: text
    std::getline(lineStream, cell, '\t');
    elements.emplace_back(
      std::make_shared<TextField>(n_body, irs::flags{irs::frequency::type(), irs::position::type(), irs::offset::type(), irs::norm::type()}, cell)
    );
  }
};

std::atomic<uint64_t> Doc::next_id(0);
const std::string& Doc::TextField::aname = std::string("text");
const std::string& Doc::TextField::aignore = std::string("{\"locale\":\"en\", \"ignored_words\":[\"abc\", \"def\", \"ghi\"]}");
const irs::text_format::type_id& Doc::TextField::aignore_format = irs::text_format::json;

struct WikiDoc : Doc {
  WikiDoc() {
    // id
    elements.emplace_back(id = std::make_shared<StringField>(n_id, irs::flags::empty_instance()));
    store.emplace_back(elements.back());

    // title: string
    elements.push_back(title = std::make_shared<StringField>(n_title, irs::flags::empty_instance()));

    // date: string
    elements.push_back(date = std::make_shared<StringField>(n_date, irs::flags::empty_instance()));
    store.push_back(elements.back());

    // date: uint64_t
    elements.push_back(ndate = std::make_shared<NumericField>(n_timesecnum, numeric_features));

    // body: text
    elements.push_back(body = std::make_shared<TextField>(n_body, text_features));
  }

  virtual void fill(std::string* line) {
    std::stringstream lineStream(*line);

    // id: uint64_t to string, base 36
    uint64_t id = next_id++; // atomic fetch and get
    char str[10];
    itoa(id, str, 36);
    char str2[10];
    snprintf(str2, sizeof (str2), "%6s", str);
    auto& s = this->id->f;
    s = str2;
    std::replace(s.begin(), s.end(), ' ', '0');

    // title: string
    std::getline(lineStream, title->f, '\t');

    // date: string
    std::getline(lineStream, date->f, '\t');

    // +date: uint64_t
    uint64_t t = 0; //boost::posix_time::microsec_clock::local_time().total_milliseconds();
    ndate->value = t;

    // body: text
    std::getline(lineStream, body->f, '\t');
  }

  std::shared_ptr<StringField> id;
  std::shared_ptr<StringField> title;
  std::shared_ptr<StringField> date;
  std::shared_ptr<NumericField> ndate;
  std::shared_ptr<TextField> body;
};

int put(
    const std::string& path,
    const std::string& dir_type,
    const std::string& format,
    std::istream& stream,
    size_t lines_max,
    size_t indexer_threads,
    size_t commit_interval_ms,
    size_t batch_size,
    bool consolidate
) {
  auto dir = create_directory(dir_type, path);

  if (!dir) {
    std::cerr << "Unable to create directory of type '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto codec = irs::formats::get(format);

  if (!codec) {
    std::cerr << "Unable to find format of type '" << format<< "'" << std::endl;
    return 1;
  }

  auto writer = irs::index_writer::make(*dir, codec, irs::OM_CREATE);

  indexer_threads = (std::max)(size_t(1), (std::min)(indexer_threads, (std::numeric_limits<size_t>::max)() - 1 - 1)); // -1 for commiter thread -1 for stream reader thread

  irs::async_utils::thread_pool thread_pool(indexer_threads + 1 + 1); // +1 for commiter thread +1 for stream reader thread

  SCOPED_TIMER("Total Time");
  std::cout << "Configuration: " << std::endl;
  std::cout << INDEX_DIR << "=" << path << std::endl;
  std::cout << DIR_TYPE << "=" << dir_type << std::endl;
  std::cout << FORMAT << "=" << format << std::endl;
  std::cout << MAX << "=" << lines_max << std::endl;
  std::cout << THR << "=" << indexer_threads << std::endl;
  std::cout << CPR << "=" << commit_interval_ms << std::endl;
  std::cout << BATCH_SIZE << "=" << batch_size << std::endl;
  std::cout << CONSOLIDATE << "=" << consolidate << std::endl;

  struct {
    std::condition_variable cond_;
    std::atomic<bool> done_;
    bool eof_;
    std::mutex mutex_;
    std::vector<std::string> buf_;

    bool swap(std::vector<std::string>& buf) {
      SCOPED_LOCK_NAMED(mutex_, lock);

      for (;;) {
        buf_.swap(buf);
        buf_.resize(0);
        cond_.notify_all();

        if (!buf.empty()) {
          return true;
        }

        if (eof_) {
          done_.store(true);
          return false;
        }

        if (!eof_) {
          SCOPED_TIMER("Stream read wait time");
          cond_.wait(lock);
        }
      }
    }
  } batch_provider;

  batch_provider.done_.store(false);
  batch_provider.eof_ = false;

  // stream reader thread
  thread_pool.run([&batch_provider, lines_max, batch_size, &stream]()->void {
    SCOPED_TIMER("Stream read total time");
    SCOPED_LOCK_NAMED(batch_provider.mutex_, lock);

    for (auto i = lines_max ? lines_max : (std::numeric_limits<size_t>::max)(); i; --i) {
      batch_provider.buf_.resize(batch_provider.buf_.size() + 1);
      batch_provider.cond_.notify_all();

      auto& line = batch_provider.buf_.back();

      if (std::getline(stream, line).eof()) {
        batch_provider.buf_.pop_back();
        break;
      }

      if (batch_size && batch_provider.buf_.size() >= batch_size) {
        SCOPED_TIMER("Stream read idle time");
        batch_provider.cond_.wait(lock);
      }
    }

    batch_provider.eof_ = true;
    std::cout << "EOF" << std::flush;
  });

  // commiter thread
  if (commit_interval_ms) {
    thread_pool.run([&batch_provider, commit_interval_ms, &writer]()->void {
      while (!batch_provider.done_.load()) {
        {
          SCOPED_TIMER("Commit time");
          std::cout << "COMMIT" << std::endl; // break indexer thread output by commit
          writer->commit();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(commit_interval_ms));
      }
    });
  }

  // indexer threads
  for (size_t i = indexer_threads; i; --i) {
    thread_pool.run([&batch_provider, &writer]()->void {
      std::vector<std::string> buf;
      WikiDoc doc;

      while (batch_provider.swap(buf)) {
        SCOPED_TIMER(std::string("Index batch ") + std::to_string(buf.size()));
        auto ctx = writer->documents();
        size_t i = 0;

        do {
          auto builder = ctx.insert();

          doc.fill(&(buf[i]));

          for (auto& field: doc.elements) {
            builder.insert(irs::action::index, *field);
          }

          for (auto& field : doc.store) {
            builder.insert(irs::action::store, *field);
          }

        } while (++i < buf.size());

        std::cout << "." << std::flush; // newline in commit thread
      }
    });
  }

  thread_pool.stop();

  {
    SCOPED_TIMER("Commit time");
    std::cout << "COMMIT" << std::endl; // break indexer thread output by commit
    writer->commit();
  }

  if (consolidate) {
    // merge all segments into a single segment

    SCOPED_TIMER("Merge time");
    std::cout << "Merging segments:" << std::endl;
    writer->consolidate(irs::index_utils::consolidation_policy(irs::index_utils::consolidate_count()));
    writer->commit();
    irs::directory_utils::remove_all_unreferenced(*dir);
  }

  u_cleanup();

  return 0;
}

int put(const cmdline::parser& args) {
  if (!args.exist(INDEX_DIR)) {
    return 1;
  }

  const auto& path = args.get<std::string>(INDEX_DIR);

  if (path.empty()) {
    return 1;
  }

  auto batch_size = args.exist(BATCH_SIZE) ? args.get<size_t>(BATCH_SIZE) : size_t(0);
  auto consolidate = args.exist(CONSOLIDATE) ? args.get<bool>(CONSOLIDATE) : false;
  auto commit_interval_ms = args.exist(CPR) ? args.get<size_t>(CPR) : size_t(0);
  auto indexer_threads = args.exist(THR) ? args.get<size_t>(THR) : size_t(0);
  auto lines_max = args.exist(MAX) ? args.get<size_t>(MAX) : size_t(0);
  auto dir_type = args.exist(DIR_TYPE) ? args.get<std::string>(DIR_TYPE) : std::string("fs");
  auto format = args.exist(FORMAT) ? args.get<std::string>(FORMAT) : std::string("1_0");

  if (args.exist(INPUT)) {
    const auto& file = args.get<std::string>(INPUT);
    std::fstream in(file, std::fstream::in);

    if (!in) {
      return 1;
    }

    return put(path, dir_type, format, in, lines_max, indexer_threads, commit_interval_ms, batch_size, consolidate);
  }

  return put(path, dir_type, format, std::cin, lines_max, indexer_threads, commit_interval_ms, batch_size, consolidate);
}

int put(int argc, char* argv[]) {
  // mode put
  cmdline::parser cmdput;
  cmdput.add(HELP, '?', "Produce help message");
  cmdput.add(INDEX_DIR, 0, "Path to index directory", true, std::string());
  cmdput.add(DIR_TYPE, 0, "Directory type (fs|mmap)", false, std::string("fs"));
  cmdput.add(FORMAT, 0, "Format (1_0|1_0-optimized)", false, std::string("1_0"));
  cmdput.add(INPUT, 0, "Input file", true, std::string());
  cmdput.add(BATCH_SIZE, 0, "Lines per batch", false, size_t(0));
  cmdput.add(CONSOLIDATE, 0, "Consolidate segments", false, false);
  cmdput.add(MAX, 0, "Maximum lines", false, size_t(0));
  cmdput.add(THR, 0, "Number of insert threads", false, size_t(0));
  cmdput.add(CPR, 0, "Commit period in lines", false, size_t(0));

  cmdput.parse(argc, argv);

  if (cmdput.exist(HELP)) {
    std::cout << cmdput.usage() << std::endl;
    return 0;
  }

  return put(cmdput);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
