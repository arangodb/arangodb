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

#include <fstream>
#include <random>
#include <thread>

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

  #include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

#include "common.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "search/bm25.hpp"
#include "search/boolean_filter.hpp"
#include "search/levenshtein_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/score.hpp"
#include "search/term_filter.hpp"
#include "search/wildcard_filter.hpp"
#include "search/ngram_similarity_filter.hpp"
#include "store/fs_directory.hpp"
#include "utils/memory_pool.hpp"
#include "utils/levenshtein_default_pdp.hpp"

#include "index-search.hpp"

// std::regex support only starting from GCC 4.9
#if !defined(__GNUC__) || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8))
  #include <regex>
#else
  #include "boost/regex.hpp"

  namespace std {
    typedef ::boost::regex regex;
    typedef ::boost::match_results<string::const_iterator> smatch;

    template <typename... Args>
    bool regex_match(Args&&... args) { return ::boost::regex_match(std::forward<Args>(args)...); }
  } // std
#endif

namespace {

const std::string HELP = "help";
const std::string INDEX_DIR = "index-dir";
const std::string OUTPUT = "out";
const std::string INPUT = "in";
const std::string MAX = "max-tasks";
const std::string THR = "threads";
const std::string TOPN = "topN";
const std::string RND = "random";
const std::string RPT = "repeat";
const std::string CSV = "csv";
const std::string SCORED_TERMS_LIMIT = "scored-terms-limit";
const std::string SCORER = "scorer";
const std::string SCORER_ARG = "scorer-arg";
const std::string SCORER_ARG_FMT = "scorer-arg-format";
const std::string DIR_TYPE = "dir-type";
const std::string FORMAT = "format";

}

enum class category_t {
  HighTerm,
  MedTerm,
  LowTerm,
  HighPhrase,
  MedPhrase,
  LowPhrase,
  AndHighHigh,
  AndHighMed,
  AndHighLow,
  OrHighHigh,
  OrHighMed,
  OrHighLow,
  Prefix3,
  Wildcard,
  Fuzzy1,
  Fuzzy2,
  Or4High,
  Or6High4Med2Low,
  MinMatch2High2Med,
  HighNGram,
  MedNGram,
  LowNGram,
  UNKNOWN
};

category_t parseCategory(const irs::string_ref& value) {
  if (value == "HighTerm") return category_t::HighTerm;
  if (value == "MedTerm") return category_t::MedTerm;
  if (value == "LowTerm") return category_t::LowTerm;
  if (value == "HighPhrase") return category_t::HighPhrase;
  if (value == "MedPhrase") return category_t::MedPhrase;
  if (value == "LowPhrase") return category_t::LowPhrase;
  if (value == "AndHighHigh") return category_t::AndHighHigh;
  if (value == "AndHighMed") return category_t::AndHighMed;
  if (value == "AndHighLow") return category_t::AndHighLow;
  if (value == "OrHighHigh") return category_t::OrHighHigh;
  if (value == "OrHighMed") return category_t::OrHighMed;
  if (value == "OrHighLow") return category_t::OrHighLow;
  if (value == "Prefix3") return category_t::Prefix3;
  if (value == "Wildcard") return category_t::Wildcard;
  if (value == "Fuzzy1") return category_t::Fuzzy1;
  if (value == "Fuzzy2") return category_t::Fuzzy2;
  if (value == "Or4High") return category_t::Or4High;
  if (value == "Or6High4Med2Low") return category_t::Or6High4Med2Low;
  if (value == "MinMatch2High2Med") return category_t::MinMatch2High2Med;
  if (value == "HighNGram") return category_t::HighNGram;
  if (value == "MedNGram") return category_t::MedNGram;
  if (value == "LowNGram") return category_t::LowNGram;
  return category_t::UNKNOWN;
}

irs::string_ref stringCategory(category_t category) {
  switch(category) {
   case category_t::HighTerm: return "HighTerm";
   case category_t::MedTerm: return "MedTerm";
   case category_t::LowTerm: return "LowTerm";
   case category_t::HighPhrase: return "HighPhrase";
   case category_t::MedPhrase: return "MedPhrase";
   case category_t::LowPhrase: return "LowPhrase";
   case category_t::AndHighHigh: return "AndHighHigh";
   case category_t::AndHighMed: return "AndHighMed";
   case category_t::AndHighLow: return "AndHighLow";
   case category_t::OrHighHigh: return "OrHighHigh";
   case category_t::OrHighMed: return "OrHighMed";
   case category_t::OrHighLow: return "OrHighLow";
   case category_t::Prefix3: return "Prefix3";
   case category_t::Wildcard: return "Wildcard";
   case category_t::Fuzzy1: return "Fuzzy1";
   case category_t::Fuzzy2: return "Fuzzy2";
   case category_t::Or4High: return "Or4High";
   case category_t::Or6High4Med2Low: return "Or6High4Med2Low";
   case category_t::MinMatch2High2Med: return "MinMatch2High2Med";
   case category_t::HighNGram: return "HighNGram";
   case category_t::MedNGram: return "MedNGram";
   case category_t::LowNGram: return "LowNGram";
   default: return "<unknown>";
  }
}

struct task_t {
  task_t(category_t category, std::string&& text) noexcept
    : category(category), text(std::move(text)) {
  }

  category_t category;
  std::string text;
};

struct timers_t {
  std::vector<irs::timer_utils::timer_stat_t*> stat;
  timers_t(const irs::string_ref& type) {
    std::string prefix("Query ");
    prefix.append(type.c_str(), type.size());
    prefix.append(" (");

    for (size_t i = 0, count = size_t(category_t::UNKNOWN); i < count; ++i) {
      stat.emplace_back(&irs::timer_utils::get_stat(prefix + stringCategory(category_t(i)).c_str() + ") time"));
    }
  }
};

irs::string_ref splitFreq(const std::string& text) {
  static const std::regex freqPattern1("(\\S+)\\s*#\\s*(.+)"); // single term, prefix
  static const std::regex freqPattern2("\"(.+)\"\\s*#\\s*(.+)"); // phrase
  static const std::regex freqPattern3("((?:\\S+\\s+)+)\\s*#\\s*(.+)"); // AND/OR groups
  std::smatch res;

  if (std::regex_match(text, res, freqPattern1)) {
    return irs::string_ref(&*(res[1].first), std::distance(res[1].first, res[1].second));
  } else if (std::regex_match(text, res, freqPattern2)) {
    return irs::string_ref(&*(res[1].first), std::distance(res[1].first, res[1].second));
  } else if (std::regex_match(text, res, freqPattern3)) {
    return irs::string_ref(&*(res[1].first), std::distance(res[1].first, res[1].second));
  }

  return irs::string_ref::NIL;
}

irs::filter::prepared::ptr prepareFilter(
    const irs::directory_reader& reader,
    const irs::order::prepared& order,
    category_t category,
    const std::string& text,
    const irs::analysis::analyzer::ptr& analyzer,
    std::string& tmpBuf,
    size_t scored_terms_limit) {
  irs::string_ref terms;

  switch (category) {
   case category_t::HighTerm: // fall through
   case category_t::MedTerm: // fall through
   case category_t::LowTerm: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::by_term query;
    *query.mutable_field() = "body";
    irs::assign(query.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(terms));

    return query.prepare(reader, order);
   }
   case category_t::HighPhrase: // fall through
   case category_t::MedPhrase: // fall through
   case category_t::LowPhrase: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::by_phrase query;
    *query.mutable_field() = "body";
    auto* opts = query.mutable_options();

    analyzer->reset(terms);

    for (auto* term = irs::get<irs::term_attribute>(*analyzer); analyzer->next();) {
      irs::assign(opts->push_back<irs::by_term_options>().term, term->value);
    }

    return query.prepare(reader, order);
   }
   case category_t::HighNGram: // fall through
   case category_t::MedNGram: // fall through
   case category_t::LowNGram: {
     if ((terms = splitFreq(text)).null()) {
       return nullptr;
     }
     irs::by_ngram_similarity query;
     *query.mutable_field() = "body";
     auto* opts = query.mutable_options();

     bool reading_threshold = true;
     // the first 'term' should be threshold in tenth - e.g. if value is 7 this means 0.7
     for (std::istringstream in(terms); std::getline(in, tmpBuf, ' ');) {
       if (reading_threshold) {
         reading_threshold = false;
         opts->threshold = float_t(std::stoll(tmpBuf))/ 10.f;
       } else {
         opts->ngrams.emplace_back(reinterpret_cast<const irs::byte_type*>(tmpBuf.c_str()),
                                   tmpBuf.size());
       }
     }
     return query.prepare(reader, order);
   }
   case category_t::AndHighHigh: // fall through
   case category_t::AndHighMed: // fall through
   case category_t::AndHighLow: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::And query;

    for (std::istringstream in(terms); std::getline(in, tmpBuf, ' ');) {
      auto& part = query.add<irs::by_term>();
      *part.mutable_field() = "body";
      irs::assign(
        part.mutable_options()->term,
        irs::ref_cast<irs::byte_type>(irs::string_ref(tmpBuf.c_str() + 1))); // +1 for skip '+' at the start of the term
    }

    return query.prepare(reader, order);
   }
   case category_t::Or4High:
   case category_t::Or6High4Med2Low:
   case category_t::OrHighHigh: // fall through
   case category_t::OrHighMed: // fall through
   case category_t::OrHighLow: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::Or query;

    for (std::istringstream in(terms); std::getline(in, tmpBuf, ' ');) {
      auto& part = query.add<irs::by_term>();
      *part.mutable_field() = "body";
      irs::assign(
        part.mutable_options()->term,
        irs::ref_cast<irs::byte_type>(tmpBuf));
    }

    return query.prepare(reader, order);
   }
   case category_t::Prefix3: {
    terms = irs::string_ref(text, text.size() - 1); // cut '~' at the end of the text

    irs::by_prefix query;
    *query.mutable_field() = "body";
    auto* opts = query.mutable_options();
    opts->scored_terms_limit = scored_terms_limit;
    irs::assign(opts->term, irs::ref_cast<irs::byte_type>(terms));

    return query.prepare(reader, order);
   }
   case category_t::Wildcard: {
    terms = irs::string_ref(text, text.size());

    irs::by_wildcard query;
    *query.mutable_field() = "body";
    auto* opts = query.mutable_options();
    irs::assign(opts->term, irs::ref_cast<irs::byte_type>(terms));
    opts->scored_terms_limit = scored_terms_limit;

    for (auto& b : opts->term) {
      switch (b) {
        case '*': b = '%'; break; // '*' => '%'
        case '?': b = '_'; break; // '?' => '_'
      }
    }


    return query.prepare(reader, order);
   }
   case category_t::Fuzzy1:
   case category_t::Fuzzy2: {
     const auto pos = text.find('~');
     const auto term = irs::string_ref(
       text.c_str(), 
       pos == std::string::npos ? text.size() : pos);

     irs::by_edit_distance query;
     *query.mutable_field() = "body";
     auto* opts = query.mutable_options();
     opts->max_terms = 50; // same as Lucene by default
     opts->max_distance = (category == category_t::Fuzzy1 ? 1 : 2);
     irs::assign(opts->term, irs::ref_cast<irs::byte_type>(term));

     return query.prepare(reader, order);
   }
   case category_t::MinMatch2High2Med: {
     if ((terms = splitFreq(text)).null()) {
       return nullptr;
     }
     irs::Or query;
     // the first 'term' should be number of minimum matched
     bool reading_min_match = true;
     for (std::istringstream in(terms); std::getline(in, tmpBuf, ' ');) {
       if (reading_min_match) {
         reading_min_match = false;
         query.min_match_count(std::stoll(tmpBuf));
       } else {
         auto& part = query.add<irs::by_term>();
         *part.mutable_field() = "body";
         irs::assign(part.mutable_options()->term,
                     irs::ref_cast<irs::byte_type>(tmpBuf));
       }
     }
     return query.prepare(reader, order);
   }
   default:
    return nullptr;
  }
}

void prepareTasks(std::vector<task_t>& buf, std::istream& in, size_t tasks_per_category) {
  std::map<category_t, size_t> category_counts;
  std::string tmpBuf;

  // parse all lines to category:text
  while (!in.eof()) {
    static const std::regex m1("(\\S+): (.+)");
    std::smatch res;

    std::getline(in, tmpBuf);

    if (std::regex_match(tmpBuf, res, m1)) {
      auto category = parseCategory(irs::string_ref(&*(res[1].first), std::distance(res[1].first, res[1].second)));
      auto& count = category_counts.emplace(category, 0).first->second;

      if (++count <= tasks_per_category) {
        buf.emplace_back(category, std::string(res[2].first, res[2].second));
      }
    }
  }
}

int search(
    const std::string& path,
    const std::string& dir_type,
    const std::string& format,
    std::istream& in,
    std::ostream& out,
    size_t tasks_max,
    size_t repeat,
    size_t search_threads,
    size_t limit,
    bool shuffle,
    bool csv,
    size_t scored_terms_limit,
    const std::string& scorer,
    const std::string& scorer_arg_format,
    const irs::string_ref& scorer_arg) {
  // build parametric descriptions for distances 1 and 2
  irs::default_pdp(1, false); irs::default_pdp(1, true);
  irs::default_pdp(2, false); irs::default_pdp(2, true);

  static const std::map<std::string, irs::type_info> text_formats = {
    { "csv", irs::type<irs::text_format::csv>::get() },
    { "json", irs::type<irs::text_format::json>::get() },
    { "text", irs::type<irs::text_format::text>::get() },
    { "xml", irs::type<irs::text_format::xml>::get() },
  };
  auto arg_format_itr = text_formats.find(scorer_arg_format);

  if (arg_format_itr == text_formats.end()) {
    std::cerr << "Unknown scorer argument format '" << scorer_arg_format << "'" << std::endl;
    return 1;
  }

  auto scr = irs::scorers::get(scorer, arg_format_itr->second, scorer_arg);

  if (!scr) {
    if (scorer_arg.null()) {
      std::cerr << "Unable to instantiate scorer '" << scorer << "' with argument format '" << scorer_arg_format << "' with nil arguments" << std::endl;
    } else {
      std::cerr << "Unable to instantiate scorer '" << scorer << "' with argument format '" << scorer_arg_format << "' with arguments '" << scorer_arg << "'" << std::endl;
    }
    return 1;
  }

  auto dir = create_directory(dir_type, path);

  if (!dir) {
    std::cerr << "Unable to create directory of type '" << dir_type << "'" << std::endl;
    return 1;
  }

  auto codec = irs::formats::get(format);

  if (!codec) {
    std::cerr << "Unable to find format of type '" << format << "'" << std::endl;
    return 1;
  }

  limit = (std::max)(size_t(1), limit); // ensure limit is greater than 0
  repeat = (std::max)(size_t(1), repeat);
  search_threads = (std::max)(size_t(1), search_threads);
  scored_terms_limit = (std::max)(size_t(1), scored_terms_limit);

  SCOPED_TIMER("Total Time");
  std::cout << "Configuration: " << std::endl;
  std::cout << INDEX_DIR << "=" << path << std::endl;
  std::cout << MAX << "=" << tasks_max << std::endl;
  std::cout << RPT << "=" << repeat << std::endl;
  std::cout << THR << "=" << search_threads << std::endl;
  std::cout << TOPN << "=" << limit << std::endl;
  std::cout << RND << "=" << shuffle << std::endl;
  std::cout << CSV << "=" << csv << std::endl;
  std::cout << SCORED_TERMS_LIMIT << "=" << scored_terms_limit << std::endl;
  std::cout << SCORER << "=" << scorer << std::endl;
  std::cout << SCORER_ARG_FMT << "=" << scorer_arg_format << std::endl;
  std::cout << SCORER_ARG << "=" << scorer_arg << std::endl;

  irs::directory_reader reader;
  irs::order::prepared order;
  irs::async_utils::thread_pool thread_pool(search_threads);

  {
    SCOPED_TIMER("Index read time");
    reader = irs::directory_reader::open(*dir, codec);
  }

  {
    SCOPED_TIMER("Order build time");
    irs::order sort;

    sort.add(true, std::move(scr));
    order = sort.prepare();
  }

  struct task_provider_t {
    typedef irs::concurrent_stack<size_t> freelist_t;

    std::mt19937 randomizer;
    std::vector<task_t> tasks;
    std::vector<freelist_t::node_type> task_ids;
    freelist_t task_list;

    void reset(std::vector<task_t>&& lines, size_t repeat, bool shuffle) {
      repeat = std::max(repeat, size_t(1)); // repeat at least once

      for (; !task_list.empty(); task_list.pop()); // empty free list

      tasks = std::move(lines);
      task_ids.resize(tasks.size()*repeat);

      for (auto begin = task_ids.begin(), end = task_ids.end(); begin != end; ) {
        for (size_t i = 0; i < tasks.size(); ++i, ++begin) {
          begin->value = i;
        }
      }

      if (shuffle) {
        for (auto begin = task_ids.begin(), end = task_ids.end(); begin != end; ) {
          auto batch_end = begin + tasks.size();
          std::shuffle(begin, batch_end, randomizer);
          begin = batch_end;
        }
      }

      // fill free list
      for (auto& id : task_ids) {
        task_list.push(id);
      }
    }

    const task_t* pop() noexcept {
      const auto* task_id = task_list.pop();

      if (!task_id) {
        return nullptr;
      }

      return &tasks[task_id->value];
    }

  } task_provider;

  // prepare tasks set
  {
    std::vector<task_t> tasks;

    prepareTasks(tasks, in, tasks_max);
    task_provider.reset(std::move(tasks), repeat, shuffle);
  }

  // indexer threads
  for (size_t i = search_threads; i; --i) {
    thread_pool.run([&task_provider, &reader, &order, limit, &out, csv, scored_terms_limit]()->void {
      static const std::string analyzer_name("text");
      static const std::string analyzer_args("{\"locale\":\"en\", \"stopwords\":[\"abc\", \"def\", \"ghi\"]}"); // from index-put
      auto analyzer = irs::analysis::analyzers::get(analyzer_name, irs::type<irs::text_format::json>::get(), analyzer_args);
      irs::filter::prepared::ptr filter;
      std::string tmpBuf;
      const timers_t building_timers("building");
      const timers_t execution_timers("execution");

      std::vector<std::pair<float_t, irs::doc_id_t>> sorted;
      sorted.reserve(limit);

      // process a single task
      for (const task_t* task; (task = task_provider.pop()) != nullptr;) {
        // this sleep circumvents context-switching penalties for CPU
        // and makes thread planner life easier
        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                static_cast<unsigned>(100. * (static_cast<double>(rand()) / static_cast<double>(RAND_MAX)))));
        size_t doc_count = 0;
        const auto start = std::chrono::system_clock::now();

        sorted.clear();

        // parse task
        {
          irs::timer_utils::scoped_timer timer(*(building_timers.stat[size_t(task->category)]));
          filter = prepareFilter(reader, order, task->category, task->text, analyzer, tmpBuf, scored_terms_limit);

          if (!filter) {
            continue;
          }
        }

        // execute task
        {
          irs::timer_utils::scoped_timer timer(*(execution_timers.stat[size_t(task->category)]));

          for (auto& segment: reader) {
            auto docs = filter->execute(segment, order); // query segment
            const irs::score* score = irs::get<irs::score>(*docs);
            assert(score);
            const irs::document* doc = irs::get<irs::document>(*docs);
            assert(doc);

            while (docs->next()) {
              ++doc_count;
              const float_t score_value = *reinterpret_cast<const float_t*>(score->evaluate());

              if (sorted.size() < limit) {
                sorted.emplace_back(score_value, doc->value);

                std::push_heap(
                  sorted.begin(), sorted.end(),
                  [](const std::pair<float_t, irs::doc_id_t>& lhs,
                     const std::pair<float_t, irs::doc_id_t>& rhs) noexcept {
                    return lhs.first < rhs.first;
                });
              } else if (sorted.front().first < score_value) {
                std::pop_heap(
                  sorted.begin(), sorted.end(),
                  [](const std::pair<float_t, irs::doc_id_t>& lhs,
                     const std::pair<float_t, irs::doc_id_t>& rhs) noexcept {
                    return lhs.first < rhs.first;
                });

                auto& back = sorted.back();
                back.first = score_value;
                back.second = doc->value;

                std::push_heap(
                  sorted.begin(), sorted.end(),
                  [](const std::pair<float_t, irs::doc_id_t>& lhs,
                     const std::pair<float_t, irs::doc_id_t>& rhs) noexcept {
                    return lhs.first < rhs.first;
                });
              }
            }
          }

          auto end = sorted.end();
          for (auto begin = sorted.begin(); begin != end; --end) {
            std::pop_heap(
              begin, end,
              [](const std::pair<float_t, irs::doc_id_t>& lhs,
                 const std::pair<float_t, irs::doc_id_t>& rhs) noexcept {
                return lhs.first < rhs.first;
            });
          }
        }

        const auto tdiff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start);

        // output task results
        {
          std::stringstream ss;
          if (csv) {
            ss << stringCategory(task->category) << "," << task->text << "," << doc_count << "," << tdiff.count() / 1000. << "," << tdiff.count() << '\n';
          } else {
            ss << "TASK: cat=" << stringCategory(task->category) << " q='body:" << task->text << "' hits=" << doc_count << '\n'
                << "  " << tdiff.count() / 1000. << " msec\n"
                << "  thread " << std::this_thread::get_id() << '\n';

            for (auto& entry : sorted) {
              ss << "  doc=" << entry.second << " score=" << entry.first << '\n';
            }

            ss << '\n';
          }

          out << ss.str();
        }
      }
    });
  }

  thread_pool.stop();

  u_cleanup();

  return 0;
}

int search(const cmdline::parser& args) {
  if (!args.exist(INDEX_DIR) || !args.exist(INPUT)) {
    return 1;
  }

  const auto& path = args.get<std::string>(INDEX_DIR);

  if (path.empty()) {
    return 1;
  }

  const size_t maxtasks = args.get<size_t>(MAX);
  const size_t repeat = args.get<size_t>(RPT);
  const bool shuffle = args.exist(RND);
  const size_t thrs = args.get<size_t>(THR);
  const size_t topN = args.get<size_t>(TOPN);
  const bool csv = args.exist(CSV);
  const size_t scored_terms_limit = args.get<size_t>(SCORED_TERMS_LIMIT);
  const auto scorer = args.get<std::string>(SCORER);
  const auto scorer_arg = args.exist(SCORER_ARG) ? irs::string_ref(args.get<std::string>(SCORER_ARG)) : irs::string_ref::NIL;
  const auto scorer_arg_format = args.get<std::string>(SCORER_ARG_FMT);
  const auto dir_type = args.exist(DIR_TYPE) ? args.get<std::string>(DIR_TYPE) : std::string("mmap");
  const auto format = args.exist(FORMAT) ? args.get<std::string>(FORMAT) : std::string("1_0");

  std::cout << "Max tasks in category="                      << maxtasks           << '\n'
            << "Task repeat count="                          << repeat             << '\n'
            << "Do task list shuffle="                       << shuffle            << '\n'
            << "Search threads="                             << thrs               << '\n'
            << "Number of top documents to collect="         << topN               << '\n'
            << "Number of terms to in range/prefix queries=" << scored_terms_limit << '\n'
            << "Scorer used for ranking query results="      << scorer             << '\n'
            << "Configuration argument format for query scorer=" << scorer_arg_format << '\n'
            << "Configuration argument for query scorer="    << scorer_arg         << '\n'
            << "Output CSV="                                 << csv                << std::endl;

  std::fstream in(args.get<std::string>(INPUT), std::fstream::in);

  if (!in) {
    return 1;
  }

  if (args.exist(OUTPUT)) {
    std::fstream out(
      args.get<std::string>(OUTPUT),
      std::fstream::out | std::fstream::trunc
    );

    if (!out) {
      return 1;
    }

    return search(path, dir_type, format, in, out, maxtasks, repeat, thrs, topN, shuffle, csv, scored_terms_limit, scorer, scorer_arg_format, scorer_arg);
  }

  return search(path, dir_type, format, in, std::cout, maxtasks, repeat, thrs, topN, shuffle, csv, scored_terms_limit, scorer, scorer_arg_format, scorer_arg);
}

int search(int argc, char* argv[]) {
  // mode search
  cmdline::parser cmdsearch;
  cmdsearch.add(HELP, '?', "Produce help message");
  cmdsearch.add<std::string>(INDEX_DIR, 0, "Path to index directory", true);
  cmdsearch.add<std::string>(DIR_TYPE, 0, "Directory type (fs|mmap)", false, std::string("mmap"));
  cmdsearch.add(FORMAT, 0, "Format (1_0|1_1|1_2|1_2simd)", false, std::string("1_0"));
  cmdsearch.add<std::string>(INPUT, 0, "Task file", true);
  cmdsearch.add<std::string>(OUTPUT, 0, "Stats file", false);
  cmdsearch.add<size_t>(MAX, 0, "Maximum tasks per category", false, size_t(1));
  cmdsearch.add<size_t>(RPT, 0, "Task repeat count", false, size_t(20));
  cmdsearch.add<size_t>(THR, 0, "Number of search threads", false, size_t(1));
  cmdsearch.add<size_t>(TOPN, 0, "Number of top search results", false, size_t(10));
  cmdsearch.add<size_t>(SCORED_TERMS_LIMIT, 0, "Number of terms to score in range/prefix queries", false, size_t(1024));
  cmdsearch.add<std::string>(SCORER, 0, "Scorer used for ranking query results", false, "bm25");
  cmdsearch.add<std::string>(SCORER_ARG, 0, "Configuration argument for query scorer", false);
  cmdsearch.add<std::string>(SCORER_ARG_FMT, 0, "Configuration argument format for query scorer", false, "json"); // 'json' is the argument format for 'bm25'
  cmdsearch.add(RND, 0, "Shuffle tasks");
  cmdsearch.add(CSV, 0, "CSV output");

  cmdsearch.parse(argc, argv);

  if (cmdsearch.exist(HELP)) {
    std::cout << cmdsearch.usage() << std::endl;
    return 0;
  }

  return search(cmdsearch);
}
