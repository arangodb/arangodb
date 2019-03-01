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
#include "search/filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/score.hpp"
#include "search/term_filter.hpp"
#include "store/fs_directory.hpp"
#include "utils/memory_pool.hpp"

#include "index-search.hpp"

// std::regex support only starting from GCC 4.9
#if !defined(__GNUC__) || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8))
  #include <regex>
#else
  #include "boost/regex.hpp"

  NS_BEGIN(std)
    typedef ::boost::regex regex;
    typedef ::boost::match_results<string::const_iterator> smatch;

    template <typename... Args>
    bool regex_match(Args&&... args) { return ::boost::regex_match(std::forward<Args>(args)...); }
  NS_END // std
#endif

NS_LOCAL

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

static bool v = false;

typedef std::unique_ptr<std::string> ustringp;

NS_END

struct Line {
    typedef std::shared_ptr<Line> ptr;
    std::string category;
    std::string text;

    Line(const std::string& c, const std::string& t): category(c), text(t) {}
};

struct Task {
    std::string category;
    std::string text;

    typedef std::shared_ptr<Task> ptr;

    irs::filter::prepared::ptr prepared;

    int taskId;
    int totalHitCount;
    int topN;

    size_t tdiff_msec;
    std::thread::id tid;

    Task(std::string& s, std::string& t, int n, irs::filter::prepared::ptr p) :
    category(s),
    text(t),
    prepared(p),
    taskId(0),
    totalHitCount(0),
    topN(n) {
    }

    virtual ~Task() {
    }

    void go(std::thread::id id, irs::directory_reader& reader) {
        tid = id;
        auto start = std::chrono::system_clock::now();

        query(reader);

        tdiff_msec = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now() - start
        ).count();
    }

    virtual int query(irs::directory_reader& reader) = 0;

    virtual void print(std::ostream& out) = 0;
    virtual void print_csv(std::ostream& out) = 0;
};

struct SearchTask : public Task {
    SearchTask(std::string& s, std::string& t, int n, irs::filter::prepared::ptr p) :
    Task(s, t, n, p) {
    }

    struct Entry {
        irs::doc_id_t id;
        float score;

        Entry(irs::doc_id_t i, float s) :
        id(i),
        score(s) {
        }
    };

    std::vector<Entry> top_docs;

    virtual int query(irs::directory_reader& reader) override {
        SCOPED_TIMER("Query execution + Result processing time");

        for (auto& segment : reader) { // iterate segments
            irs::order order;
            order.add<irs::bm25_sort>(true);
            auto prepared_order = order.prepare();
            auto docs = prepared->execute(segment, prepared_order); // query segment
            const irs::score* score = docs->attributes().get<iresearch::score>().get();
            auto comparer = [&prepared_order](const irs::bstring& lhs, const irs::bstring& rhs)->bool {
              return prepared_order.less(lhs.c_str(), rhs.c_str());
            };
            std::multimap<irs::bstring, Entry, decltype(comparer)> sorted(comparer);

            // ensure we avoid COW for pre c++11 std::basic_string
            const irs::bytes_ref score_value = score->value();

            while (docs->next()) {
              SCOPED_TIMER("Result processing time");
              ++totalHitCount;
              score->evaluate();
              sorted.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(score_value),
                std::forward_as_tuple(docs->value(), score ? prepared_order.get<float>(score_value.c_str(), 0) : .0)
              );

              if (sorted.size() > topN) {
                sorted.erase(--(sorted.end()));
              }
            }

            for (auto& entry: sorted) {
              top_docs.emplace_back(std::move(entry.second));
            }
        }
        return 0;
    }

    void print(std::ostream& out) override {
        out << "TASK: cat=" << category << " q='body:" << text << "' hits=" << totalHitCount << std::endl;
        out << "  " << tdiff_msec / 1000. << " msec" << std::endl;
        out << "  thread " << tid << std::endl;
        for (auto& doc : top_docs) {
            out << "  doc=" << doc.id << " score=" << doc.score << std::endl;
        }
        out << std::endl;
    }

    void print_csv(std::ostream& out) override {
        out << category << "," << text << "," << totalHitCount << "," << tdiff_msec / 1000. << "," << tdiff_msec << std::endl;
    }
};

class TaskSource {
    std::atomic<int> idx;
    std::vector<Task::ptr> tasks;
    std::random_device rd;
    std::mt19937 g;

    int parseLines(std::string& line, Line::ptr& p) {
        static const std::regex m1("(\\S+): (.+)");
        std::smatch res;
        std::string category;
        std::string text;

        if (std::regex_match(line, res, m1)) {
            category.assign(res[1].first, res[1].second);
            text.assign(res[2].first, res[2].second);
            p = Line::ptr(new Line(category, text));

            return 0;
        }

        return -1;
    }

    int loadLines(std::vector<Line::ptr>& lines, std::istream& stream) {
        while (!stream.eof()) {
            std::string line;
            std::getline(stream, line);
            Line::ptr p;

            if (0 == parseLines(line, p)) {
                lines.push_back(p);
            }
        }

        return 0;
    }

    void shuffle(std::vector<Line::ptr>& line) {
        // @todo provide custom random?
        std::shuffle(line.begin(), line.end(), g);
    }

    static int pruneLines(std::vector<Line::ptr>& lines, std::vector<Line::ptr>& pruned_lines, int maxtasks) {
        std::map<std::string, int> cat_counts;

        for (auto& t : lines) {
            std::map<std::string, int>::iterator cat = cat_counts.find(t->category);
            int count = 0;

            if (cat != cat_counts.end()) {
                count = cat->second;
            }

            if (count < maxtasks) {
                ++count;

                if (cat != cat_counts.end()) {
                    cat->second = count;
                } else {
                    cat_counts[t->category] = count;
                }

                pruned_lines.push_back(t);
            }
        }

        return 0;
    }

    bool splitFreq(std::string& text, std::string& term) {
        static const std::regex freqPattern1("(\\S+)\\s*#\\s*(.+)"); // single term, prefix
        static const std::regex freqPattern2("\"(.+)\"\\s*#\\s*(.+)"); // phrase
        static const std::regex freqPattern3("((?:\\S+\\s+)+)\\s*#\\s*(.+)"); // AND/OR groups
        std::smatch res;

        if (std::regex_match(text, res, freqPattern1)) {
            term.assign(res[1].first, res[1].second);
            return true;
        } else if (std::regex_match(text, res, freqPattern2)) {
            term.assign(res[1].first, res[1].second);
            return true;
        } else if (std::regex_match(text, res, freqPattern3)) {
            term.assign(res[1].first, res[1].second);
            return true;
        }

        return false;
    }

    int prepareQueries(std::vector<Line::ptr>& lines, irs::directory_reader& reader, int topN) {
        irs::order order;
        order.add<irs::bm25_sort>(true);
        auto ord = order.prepare();

        for (auto& line : lines) {
            irs::filter::prepared::ptr prepared = nullptr;
            std::string terms;

            if (line->category == "HighTerm" || line->category == "MedTerm" || line->category == "LowTerm") {
                if (splitFreq(line->text, terms)) {
                    irs::by_term query;
                    query.field("body").term(terms);
                    prepared = query.prepare(reader, ord);
                }
            } else if (line->category == "HighPhrase" || line->category == "MedPhrase" || line->category == "LowPhrase") {
                // @todo what's the difference between irs::by_phrase and irs::And?
                if (splitFreq(line->text, terms)) {
                    std::istringstream f(terms);
                    std::string term;
                    irs::by_phrase query;
                    query.field("body");
                    while (getline(f, term, ' ')) {
                        query.push_back(term);
                    }
                    prepared = query.prepare(reader, ord);
                }
            } else if (line->category == "AndHighHigh" || line->category == "AndHighMed" || line->category == "AndHighLow") {
                if (splitFreq(line->text, terms)) {
                    std::istringstream f(terms);
                    std::string term;
                    irs::And query;
                    while (getline(f, term, ' ')) {
                        irs::by_term& part = query.add<irs::by_term>();
                        part.field("body").term(term.c_str() + 1); // skip '+' at the start of the term
                    }
                    prepared = query.prepare(reader, ord);
                }
            } else if (line->category == "OrHighHigh" || line->category == "OrHighMed" || line->category == "OrHighLow") {
                if (splitFreq(line->text, terms)) {
                    std::istringstream f(terms);
                    std::string term;
                    irs::Or query;
                    while (getline(f, term, ' ')) {
                        irs::by_term& part = query.add<irs::by_term>();
                        part.field("body").term(term);
                    }
                    prepared = query.prepare(reader, ord);
                }
            } else if (line->category == "Prefix3") {
                irs::by_prefix query;
                terms.assign(line->text.begin(), line->text.end() - 1); // cut '~' at the end of the text
                query.field("body").term(terms);
                prepared = query.prepare(reader, ord);
            }

            if (prepared != nullptr) {
                tasks.emplace_back(new SearchTask(line->category, terms, topN, prepared));
                if (v) std::cout << tasks.size() << ": cat=" << line->category << "; term=" << terms << std::endl;
            }
        }

        if (v) std::cout << "Tasks prepared=" << tasks.size() << std::endl;

        return 0;
    }

    int repeatLines(std::vector<Line::ptr>& lines, std::vector<Line::ptr>& rep_lines, int repeat, bool do_shuffle) {
        while (repeat != 0) {
            if (do_shuffle) {
                shuffle(lines);
            }

            rep_lines.insert(std::end(rep_lines), std::begin(lines), std::end(lines));
            --repeat;
        }

        return 0;
    }

public:
    TaskSource() : idx(0), g(rd()) {
    }

    int load(std::istream& stream, int maxtasks, int repeat, irs::directory_reader& reader, int topN, bool do_shuffle) {
        /// 
        ///  this fn mimics lucene-util's LocalTaskSource behavior
        ///  -- many similar tasks generated 
        ///
        std::vector<Line::ptr> rep_lines;
        {
            std::vector<Line::ptr> pruned_lines;
            {
                std::vector<Line::ptr> lines;
                // parse all lines to category:text
                loadLines(lines, stream);

                // shuffle
                if (do_shuffle) {
                    shuffle(lines);
                }

                // prune tasks
                pruneLines(lines, pruned_lines, maxtasks);
            }

            // multiply pruned with shuffling
            repeatLines(pruned_lines, rep_lines, repeat, do_shuffle);
        }

        // prepare queries
        prepareQueries(rep_lines, reader, topN);

        return 0;
    }

    Task::ptr next() {
        int next = idx++; // atomic get and increment

        if (next < tasks.size()) {
            return tasks[next];
        }

        return nullptr;
    }

    std::vector<Task::ptr>& getTasks() {
        return tasks;
    }
};

class TaskThread {
public:
    typedef std::shared_ptr<TaskThread> ptr;

    struct Args {
        TaskSource& tasks;
        irs::directory_reader& reader;

        Args(TaskSource& t,
                irs::directory_reader& r) :
        tasks(t),
        reader(r) {
        }
    };

private:
    std::thread* thr;

    void worker(Args a) {
        auto task = a.tasks.next();

        while (task != nullptr) {
            task->go(thr->get_id(), a.reader);
            task = a.tasks.next();
        }
    }

public:
    void start(Args a) {
        thr = new std::thread(std::bind(&TaskThread::worker, this, a));
    }

    void join() {
        thr->join();
        delete thr;
    }
};

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
  UNKNOWN
};

struct task_t {
  category_t category;
  std::string text;
  task_t(category_t v_category, std::string&& v_text): category(v_category), text(std::move(v_text)) {}
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
   default: return "<unknown>";
  }
}

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
    size_t scored_terms_limit
) {
  irs::string_ref terms;

  switch (category) {
   case category_t::HighTerm: // fall through
   case category_t::MedTerm: // fall through
   case category_t::LowTerm: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::by_term query;

    query.field("body").term(terms);

    return query.prepare(reader, order);
   }
   case category_t::HighPhrase: // fall through
   case category_t::MedPhrase: // fall through
   case category_t::LowPhrase: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::by_phrase query;

    query.field("body");
    analyzer->reset(terms);

    for (auto& term = analyzer->attributes().get<irs::term_attribute>(); analyzer->next();) {
      query.push_back(term->value());
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
      query.add<irs::by_term>().field("body").term(tmpBuf.c_str() + 1); // +1 for skip '+' at the start of the term
    }

    return query.prepare(reader, order);
   }
   case category_t::OrHighHigh: // fall through
   case category_t::OrHighMed: // fall through
   case category_t::OrHighLow: {
    if ((terms = splitFreq(text)).null()) {
      return nullptr;
    }

    irs::Or query;

    for (std::istringstream in(terms); std::getline(in, tmpBuf, ' ');) {
      query.add<irs::by_term>().field("body").term(tmpBuf);
    }

    return query.prepare(reader, order);
   }
   case category_t::Prefix3: {
    irs::by_prefix query;
    query.scored_terms_limit(scored_terms_limit);

    terms = irs::string_ref(text, text.size() - 1); // cut '~' at the end of the text
    query.field("body").term(terms);

    return query.prepare(reader, order);
   }
  }

  return nullptr;
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

static int testQueries(std::vector<Task::ptr>& tasks, irs::directory_reader& reader) {
    for (auto& segment : reader) { // iterate segments
        int cnt = 0;

        for (auto& task : tasks) {
            ++cnt;
            std::cout << "running query=" << cnt << std::endl;

            auto& query = task->prepared;
            auto docs = query->execute(segment); // query segment

            while (docs->next()) {
                const irs::doc_id_t doc_id = docs->value(); // get doc id

                std::cout << cnt << " : " << doc_id << std::endl;
            }
        }
    }

    return 0;
}

static int printResults(std::vector<Task::ptr>& tasks, std::ostream& out, bool csv) {
    for (auto& task : tasks) {
        csv ? task->print_csv(out) : task->print(out);
    }
    return 0;
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
    const irs::string_ref& scorer_arg
) {
  static const std::map<std::string, const irs::text_format::type_id&> text_formats = {
    { "csv", irs::text_format::csv },
    { "json", irs::text_format::json },
    { "text", irs::text_format::text },
    { "xml", irs::text_format::xml },
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

    sort.add(true, scr);
    order = sort.prepare();
  }

  struct task_provider_t {
    std::mutex mutex;
    size_t next_task;
    std::mt19937 randomizer;
    size_t repeat;
    bool shuffle;
    std::vector<task_t> tasks;
    std::vector<size_t> task_ids;

    task_provider_t(): repeat(0), shuffle(false) {}

    void operator=(std::vector<task_t>&& lines) {
      next_task = 0;
      tasks = std::move(lines);
      task_ids.resize(tasks.size());

      for (size_t i = 0, count = task_ids.size(); i < count; ++i) {
        task_ids[i] = i;
      }

      // initial shuffle
      if (shuffle) {
        std::shuffle(task_ids.begin(), task_ids.end(), randomizer);
      }
    }

    const task_t* operator++() {
      SCOPED_LOCK(mutex);

      if (next_task >= task_ids.size()) {
        return nullptr;
      }

      auto& task = tasks[task_ids[next_task++]];

      // prepare tasks for next iteration if repeat requested
      if (next_task >= task_ids.size() && repeat) {
        next_task = 0;
        --repeat;

        // shuffle
        if (shuffle) {
          std::shuffle(task_ids.begin(), task_ids.end(), randomizer);
        }
      }

      return &task;
    }

  } task_provider;

  // prepare tasks set
  {
    std::vector<task_t> tasks;

    prepareTasks(tasks, in, tasks_max);
    task_provider.repeat = repeat - 1; // -1 for first run (i.e. additional repeats)
    task_provider.shuffle = shuffle;
    task_provider = std::move(tasks);
  }

  struct Entry {
    Entry(irs::doc_id_t i, float s)
      : id(i), score(s) {
    }

    irs::doc_id_t id;
    float score;
  };

  // indexer threads
  for (size_t i = search_threads; i; --i) {
    thread_pool.run([&task_provider, &dir, &reader, &order, limit, &out, csv, scored_terms_limit]()->void {
      static const std::string analyzer_name("text");
      static const std::string analyzer_args("{\"locale\":\"en\", \"ignored_words\":[\"abc\", \"def\", \"ghi\"]}"); // from index-put
      auto analyzer = irs::analysis::analyzers::get(analyzer_name, irs::text_format::json, analyzer_args);
      irs::filter::prepared::ptr filter;
      std::string tmpBuf;

      #if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
        typedef irs::memory::memory_multi_size_pool<irs::memory::identity_grow> pool_t;
        typedef irs::memory::memory_pool_multi_size_allocator<Entry, pool_t> alloc_t;
      #else
        typedef irs::memory::memory_pool<irs::memory::identity_grow> pool_t;
        typedef irs::memory::memory_pool_allocator<Entry, pool_t> alloc_t;
      #endif

      pool_t pool(limit + 1); // +1 for least significant overflow element

#ifdef IRESEARCH_COMPLEX_SCORING
      auto comparer = [&order](const irs::bstring& lhs, const irs::bstring& rhs)->bool {
        return order.less(lhs.c_str(), rhs.c_str());
      };
      std::multimap<irs::bstring, Entry, decltype(comparer), alloc_t> sorted(
        comparer, alloc_t{pool}
      );
#else
      std::multimap<float, irs::doc_id_t, std::less<float>, alloc_t> sorted(
        std::less<float>(), alloc_t{pool}
      );
#endif

      // process a single task
      for (const task_t* task; (task = ++task_provider) != nullptr;) {
        SCOPED_TIMER("Full task processing time");
        size_t doc_count = 0;
        auto start = std::chrono::system_clock::now();

        sorted.clear();

        // parse task
        {
          static struct timers_t {
            std::vector<irs::timer_utils::timer_stat_t*> stat;
            timers_t() {
              for (size_t i = 0, count = size_t(category_t::UNKNOWN); i <= count; ++i) {
                stat.emplace_back(&irs::timer_utils::get_stat(std::string("Query building (") + stringCategory(category_t(i)).c_str() + ") time"));
              }
            }
          } timers;
          SCOPED_TIMER("Query building time");
          irs::timer_utils::scoped_timer timer(*(timers.stat[size_t(task->category)]));
          filter = prepareFilter(reader, order, task->category, task->text, analyzer, tmpBuf, scored_terms_limit);

          if (!filter) {
            continue;
          }
        }

        // execute task
        {
          static struct timers_t {
            std::vector<irs::timer_utils::timer_stat_t*> stat;
            timers_t() {
              for (size_t i = 0, count = size_t(category_t::UNKNOWN); i <= count; ++i) {
                stat.emplace_back(&irs::timer_utils::get_stat(std::string("Query execution (") + stringCategory(category_t(i)).c_str() + ") time"));
              }
            }
          } timers;
          SCOPED_TIMER("Query execution time");
          irs::timer_utils::scoped_timer timer(*(timers.stat[size_t(task->category)]));

          const float EMPTY_SCORE = 0.f;

          for (auto& segment: reader) {
            auto docs = filter->execute(segment, order); // query segment
            const irs::score& score = irs::score::extract(docs->attributes());

#ifdef IRESEARCH_COMPLEX_SCORING
            // ensure we avoid COW for pre c++11 std::basic_string
            const irs::bytes_ref raw_score_value = score->value();                        
#endif
            const auto& score_value = &score != &irs::score::no_score()
              ? order.get<float>(score.c_str(), 0)
              : EMPTY_SCORE;
            
            while (docs->next()) {
              ++doc_count;
              score.evaluate();

#ifdef IRESEARCH_COMPLEX_SCORING
              sorted.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(raw_score_value),
                std::forward_as_tuple(docs->value(), score_value)
              );
#else
              sorted.emplace(score_value, docs->value());
#endif

              if (sorted.size() > limit) {
                sorted.erase(--(sorted.end()));
              }
            }
          }
        }

        // output task results
        {
          static std::mutex mutex;
          SCOPED_LOCK(mutex);
          SCOPED_TIMER("Result processing time");
          auto tdiff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start);

          if (csv) {
            out << stringCategory(task->category) << "," << task->text << "," << doc_count << "," << tdiff.count() / 1000. << "," << tdiff.count() << std::endl;
          } else {
            out << "TASK: cat=" << stringCategory(task->category) << " q='body:" << task->text << "' hits=" << doc_count << std::endl;
            out << "  " << tdiff.count() / 1000. << " msec" << std::endl;
            out << "  thread " << std::this_thread::get_id() << std::endl;

            for (auto& entry : sorted) {
#ifdef IRESEARCH_COMPLEX_SCORING
              out << "  doc=" << entry.second.id << " score=" << entry.second.score << std::endl;
#else
              out << "  doc=" << entry.second << " score=" << entry.first<< std::endl;
#endif
            }

            out << std::endl;
          }
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
  const auto dir_type = args.exist(DIR_TYPE) ? args.get<std::string>(DIR_TYPE) : std::string("fs");
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
  cmdsearch.add<std::string>(DIR_TYPE, 0, "Directory type (fs|mmap)", false, std::string("fs"));
  cmdsearch.add(FORMAT, 0, "Format (1_0|1_0-optimized)", false, std::string("1_0"));
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------