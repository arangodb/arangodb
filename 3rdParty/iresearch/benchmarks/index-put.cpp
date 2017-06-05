//
// IResearch search engine 
// 
// Copyright (c) 2017 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "index/index_writer.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/text_token_stream.hpp"
#include "store/fs_directory.hpp"
#include "analysis/token_attributes.hpp"

#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <unicode/uclean.h>

#include <fstream>
#include <iostream>

namespace {

namespace po = boost::program_options;

}

static const std::string BATCH_SIZE = "batch-size";
static const std::string CONSOLIDATE = "consolidate";
static const std::string INDEX_DIR = "index-dir";
static const std::string OUTPUT = "out";
static const std::string INPUT = "in";
static const std::string MAX = "max-lines";
static const std::string THR = "threads";
static const std::string CPR = "commit-period";

typedef std::unique_ptr<std::string> ustringp;

#if defined(_MSC_VER) && (_MSC_VER < 1900)
  #define snprintf _snprintf
#endif

static const std::string n_id = "id";
static const std::string n_title = "title";
static const std::string n_date = "date";
static const std::string n_timesecnum = "timesecnum";
static const std::string n_body = "body";

static const irs::flags text_features{
  irs::frequency::type(),
  irs::position::type(),
  irs::offset::type(),
  irs::norm::type()
};

static const irs::flags numeric_features{
  irs::granularity_prefix::type()
};


/**
 * Document
 */
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

    TextField(const std::string& n, const irs::flags& flags)
      : Field(n, flags) {
      stream = irs::analysis::analyzers::get(aname, aignore);
    }

    TextField(const std::string& n, const irs::flags& flags, std::string& a)
      : Field(n, flags), f(a) {
      stream = irs::analysis::analyzers::get(aname, aignore);
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
    std::istream& stream,
    size_t lines_max,
    size_t indexer_threads,
    size_t commit_interval_ms,
    size_t batch_size,
    bool consolidate
) {
  irs::fs_directory dir(path);
  auto writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OPEN_MODE::OM_CREATE);

  indexer_threads = std::max(size_t(1), std::min(indexer_threads, std::numeric_limits<size_t>::max() - 1 - 1)); // -1 for commiter thread -1 for stream reader thread

  irs::async_utils::thread_pool thread_pool(indexer_threads + 1 + 1); // +1 for commiter thread +1 for stream reader thread

  SCOPED_TIMER("Total Time");
  std::cout << "Configuration: " << std::endl;
  std::cout << INDEX_DIR << "=" << path << std::endl;
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

    for (auto i = lines_max ? lines_max : std::numeric_limits<size_t>::max(); i; --i) {
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
        size_t i = 0;
        auto inserter = [&buf, &i, &doc](const irs::index_writer::document& builder) {
          doc.fill(&(buf[i]));

          for (auto& field: doc.elements) {
            builder.insert<irs::Action::INDEX>(*field);
          }

          for (auto& field : doc.store) {
            builder.insert<irs::Action::STORE>(*field);
          }

          return ++i < buf.size();
        };

        writer->insert(inserter);
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

  // merge all segments into a single segment
  writer->consolidate(
    [](const irs::directory&, const irs::index_meta&)->irs::index_writer::consolidation_acceptor_t {
      return [](const irs::segment_meta& meta)->bool {
        std::cout << meta.name << std::endl;
        return true;
      };
    },
    false
  );

  if (consolidate) {
    SCOPED_TIMER("Merge time");
    std::cout << "Merging segments:" << std::endl;
    writer->commit();
    irs::directory_utils::remove_all_unreferenced(dir);
  }

  u_cleanup();

  return 0;
}

/**
 * 
 * @param vm
 * @return 
 */
int put(const po::variables_map& vm) {
    if (!vm.count(INDEX_DIR)) {
        return 1;
    }

    auto& path = vm[INDEX_DIR].as<std::string>();

    if (path.empty()) {
        return 1;
    }

    auto batch_size = vm.count(BATCH_SIZE) ? vm[BATCH_SIZE].as<size_t>() : size_t(0);
    auto consolidate = vm.count(CONSOLIDATE) ? vm[CONSOLIDATE].as<bool>() : false;
    auto commit_interval_ms = vm.count(CPR) ? vm[CPR].as<size_t>() : size_t(0);
    auto indexer_threads = vm.count(THR) ? vm[THR].as<size_t>() : size_t(0);
    auto lines_max = vm.count(MAX) ? vm[MAX].as<size_t>() : size_t(0);

    if (vm.count(INPUT)) {
        auto& file = vm[INPUT].as<std::string>();
        std::fstream in(file, std::fstream::in);
        if (!in) {
            return 1;
        }

        return put(path, in, lines_max, indexer_threads, commit_interval_ms, batch_size, consolidate);
    }

    return put(path, std::cin, lines_max, indexer_threads, commit_interval_ms, batch_size, consolidate);
}

/**
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char* argv[]) {
    irs::logger::output_le(iresearch::logger::IRL_ERROR, stderr);

    po::variables_map vm;

    // general description
    std::string mode;
    po::options_description desc("\n[IReSearch-benchmarks-index] General options");
    desc.add_options()
            ("help,h", "produce help message")
            ("mode,m", po::value<std::string>(&mode), "Select mode: put");

    // stats mode description
    po::options_description put_desc("Put mode options");
    put_desc.add_options()
            (INDEX_DIR.c_str(), po::value<std::string>(), "Path to index directory")
            (INPUT.c_str(), po::value<std::string>(), "Input file")
            (BATCH_SIZE.c_str(), po::value<size_t>(), "Lines per batch")
            (CONSOLIDATE.c_str(), po::value<bool>(), "Consolidate segments")
            (MAX.c_str(), po::value<size_t>(), "Maximum lines")
            (THR.c_str(), po::value<size_t>(), "Number of insert threads")
            (CPR.c_str(), po::value<size_t>(), "Commit period in lines");

    po::command_line_parser parser(argc, argv);
    parser.options(desc).allow_unregistered();
    po::parsed_options options = parser.run();
    po::store(options, vm);
    po::notify(vm);

    // show help
    if (vm.count("help")) {
        desc.add(put_desc);
        std::cout << desc << std::endl;
        return 0;
    }

    irs::timer_utils::init_stats(true);
    auto output_stats = irs::make_finally([]()->void {
      irs::timer_utils::visit([](const std::string& key, size_t count, size_t time)->bool {
        std::cout << key << " calls:" << count << ", time: " << time/1000 << " us, avg call: " << time/1000/(double)count << " us"<< std::endl;
        return true;
      });
    });

    // enter dump mode
    if ("put" == mode) {
        desc.add(put_desc);
        po::store(po::parse_command_line(argc, argv, desc), vm);

        return put(vm);
    }

    return 0;
}
