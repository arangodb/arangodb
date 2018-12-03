//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// @author Vasily Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
  #include "date/date.h"
  #undef NOEXCEPT
#endif

#include "search/scorers.hpp"
#include "utils/log.hpp"

#include "ApplicationServerHelper.h"
#include "Containers.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchMMFilesLink.h"
#include "IResearchRocksDBLink.h"
#include "IResearchLinkCoordinator.h"
#include "IResearchLinkHelper.h"
#include "IResearchRocksDBRecoveryHelper.h"
#include "IResearchView.h"
#include "IResearchViewCoordinator.h"
#include "IResearchViewDBServer.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Function.h"
#include "Basics/ConditionLocker.h"
#include "Basics/SmallVector.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "RestServer/ViewTypesFeature.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "MMFiles/MMFilesEngine.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalView.h"

NS_BEGIN(arangodb)

NS_BEGIN(basics)
class VPackStringBufferAdapter;
NS_END // basics

NS_BEGIN(aql)
class Query;
NS_END // aql

NS_END // arangodb

NS_LOCAL

class IResearchLogTopic final : public arangodb::LogTopic {
 public:
  IResearchLogTopic(std::string const& name)
    : arangodb::LogTopic(name, DEFAULT_LEVEL) {
    setIResearchLogLevel(DEFAULT_LEVEL);
  }

  virtual void setLogLevel(arangodb::LogLevel level) override {
    arangodb::LogTopic::setLogLevel(level);
    setIResearchLogLevel(level);
  }

 private:
  static arangodb::LogLevel const DEFAULT_LEVEL= arangodb::LogLevel::INFO;

  typedef std::underlying_type<irs::logger::level_t>::type irsLogLevelType;
  typedef std::underlying_type<arangodb::LogLevel>::type arangoLogLevelType;

  static_assert(
    static_cast<irsLogLevelType>(irs::logger::IRL_FATAL) == static_cast<arangoLogLevelType>(arangodb::LogLevel::FATAL) - 1
      && static_cast<irsLogLevelType>(irs::logger::IRL_ERROR) == static_cast<arangoLogLevelType>(arangodb::LogLevel::ERR) - 1
      && static_cast<irsLogLevelType>(irs::logger::IRL_WARN) == static_cast<arangoLogLevelType>(arangodb::LogLevel::WARN) - 1
      && static_cast<irsLogLevelType>(irs::logger::IRL_INFO) == static_cast<arangoLogLevelType>(arangodb::LogLevel::INFO) - 1
      && static_cast<irsLogLevelType>(irs::logger::IRL_DEBUG) == static_cast<arangoLogLevelType>(arangodb::LogLevel::DEBUG) - 1
      && static_cast<irsLogLevelType>(irs::logger::IRL_TRACE) == static_cast<arangoLogLevelType>(arangodb::LogLevel::TRACE) - 1,
    "inconsistent log level mapping"
  );

  static void setIResearchLogLevel(arangodb::LogLevel level) {
    if (level == arangodb::LogLevel::DEFAULT) {
      level = DEFAULT_LEVEL;
    }

    auto irsLevel = static_cast<irs::logger::level_t>(
      static_cast<arangoLogLevelType>(level) - 1
    ); // -1 for DEFAULT

    irsLevel = std::max(irsLevel, irs::logger::IRL_FATAL);
    irsLevel = std::min(irsLevel, irs::logger::IRL_TRACE);

    irs::logger::output_le(irsLevel, stderr);
  }
}; // IResearchLogTopic

//template <char const *name>
arangodb::aql::AqlValue dummyFilterFunc(
    arangodb::aql::ExpressionContext*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "ArangoSearch filter functions EXISTS, STARTS_WITH, PHRASE, MIN_MATCH, BOOST and ANALYZER "
    " are designed to be used only within a corresponding SEARCH statement of ArangoSearch view."
    " Please ensure function signature is correct.");
}

arangodb::aql::AqlValue dummyScorerFunc(
    arangodb::aql::ExpressionContext*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "ArangoSearch scorer functions BM25() and TFIDF() are designed to "
    "be used only outside SEARCH statement within a context of ArangoSearch view."
    " Please ensure function signature is correct."
  );
}

size_t computeThreadPoolSize(size_t threads, size_t threadsLimit) {
  static const size_t MAX_THREADS = 8; // arbitrary limit on the upper bound of threads in pool
  static const size_t MIN_THREADS = 1; // at least one thread is required
  auto maxThreads = threadsLimit ? threadsLimit : MAX_THREADS;

  return threads
    ? threads
    : std::max(
        MIN_THREADS,
        std::min(maxThreads, size_t(std::thread::hardware_concurrency()) / 4)
      )
    ;
}

void registerFunctions(arangodb::aql::AqlFunctionFeature& /*functions*/) {
#if 0
  arangodb::iresearch::addFunction(functions, {
    "__ARANGOSEARCH_SCORE_DEBUG",  // name
    ".",    // value to convert
    arangodb::aql::Function::makeFlags(
      arangodb::aql::Function::Flags::Deterministic,
      arangodb::aql::Function::Flags::Cacheable,
      arangodb::aql::Function::Flags::CanRunOnDBServer
    ),
    [](arangodb::aql::ExpressionContext*,
       arangodb::transaction::Methods*,
       arangodb::SmallVector<arangodb::aql::AqlValue> const& args) {
      if (args.empty()) {
        // no such input parameter. return NaN
        return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble(double_t(std::nan(""))));
      } else {
        // unsafe
        VPackValueLength length;
        auto const floatValue = *reinterpret_cast<float_t const*>(args[0].slice().getString(length));
        return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble(double_t(floatValue)));
      }
    }
  });
#endif
}

void registerFilters(arangodb::aql::AqlFunctionFeature& functions) {
  using arangodb::iresearch::addFunction;
  auto flags = arangodb::aql::Function::makeFlags(
                 arangodb::aql::Function::Flags::Deterministic,
                 arangodb::aql::Function::Flags::Cacheable,
                 arangodb::aql::Function::Flags::CanRunOnDBServer);
  addFunction(functions, {"EXISTS", ".|.,.", flags, &dummyFilterFunc}); // (attribute, [ "analyzer"|"type"|"string"|"numeric"|"bool"|"null" ])
  addFunction(functions, {"STARTS_WITH", ".,.|.", flags, &dummyFilterFunc}); // (attribute, prefix, scoring-limit)
  addFunction(functions, {"PHRASE", ".,.|.+", flags, &dummyFilterFunc}); // (attribute, input [, offset, input... ] [, analyzer])
  addFunction(functions, {"MIN_MATCH", ".,.|.+", flags, &dummyFilterFunc}); // (filter expression [, filter expression, ... ], min match count)
  addFunction(functions, {"BOOST", ".,.", flags, &dummyFilterFunc}); // (filter expression, boost)
  addFunction(functions, {"ANALYZER", ".,.", flags, &dummyFilterFunc}); // (filter expression, analyzer)
}

void registerIndexFactory() {
  static const std::map<std::string, arangodb::IndexTypeFactory const*> factories = {
    { "ClusterEngine", &arangodb::iresearch::IResearchLinkCoordinator::factory() },
    { "MMFilesEngine", &arangodb::iresearch::IResearchMMFilesLink::factory() },
    { "RocksDBEngine", &arangodb::iresearch::IResearchRocksDBLink::factory() }
  };
  auto const& indexType = arangodb::iresearch::DATA_SOURCE_TYPE.name();

  // register 'arangosearch' link
  for (auto& entry: factories) {
    auto* engine = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::StorageEngine
    >(entry.first);

    // valid situation if not running with the specified storage engine
    if (!engine) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failed to find feature '" << entry.first << "' while registering index type '" << indexType << "', skipping";
      continue;
    }

    // ok to const-cast since this should only be called on startup
    auto& indexFactory =
      const_cast<arangodb::IndexFactory&>(engine->indexFactory());
    auto res = indexFactory.emplace(indexType, *(entry.second));

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering IResearch link factory with index factory from feature '") + entry.first + "': " + res.errorMessage()
      );
    }
  }
}

void registerScorers(arangodb::aql::AqlFunctionFeature& functions) {
  irs::string_ref const args(".|+"); // positional arguments (attribute [, <scorer-specific properties>...]);

  irs::scorers::visit([&functions, &args](
     irs::string_ref const& name, irs::text_format::type_id const& args_format
  )->bool {
    // ArangoDB, for API consistency, only supports scorers configurable via jSON
    if (irs::text_format::json != args_format) {
      return true;
    }

    std::string upperName = name;

    // AQL function external names are always in upper case
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    arangodb::iresearch::addFunction(functions, {
      std::move(upperName),
      args.c_str(),
      arangodb::aql::Function::makeFlags(
        arangodb::aql::Function::Flags::Deterministic,
        arangodb::aql::Function::Flags::Cacheable,
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      &dummyScorerFunc // function implementation
    });

    return true;
  });
}

void registerRecoveryHelper() {
  auto helper = std::make_shared<arangodb::iresearch::IResearchRocksDBRecoveryHelper>();
  auto res = arangodb::RocksDBEngine::registerRecoveryHelper(helper);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), "failed to register RocksDB recovery helper");
  }
}

void registerViewFactory() {
  auto& viewType = arangodb::iresearch::DATA_SOURCE_TYPE;
  auto* viewTypes = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::ViewTypesFeature
  >();

  if (!viewTypes) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string("failed to find feature '") + arangodb::ViewTypesFeature::name() + "' while registering view type '" + viewType.name() + "'"
    );
  }

  arangodb::Result res;

  // DB server in custer or single-server
  if (arangodb::ServerState::instance()->isCoordinator()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewCoordinator::factory());
  } else if (arangodb::ServerState::instance()->isDBServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewDBServer::factory());
  } else if (arangodb::ServerState::instance()->isSingleServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchView::factory());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_FAILED,
      std::string("Invalid role for arangosearch view creation.")
    );
  }

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      res.errorNumber(),
      std::string("failure registering arangosearch view factory: ") + res.errorMessage()
    );
  }
}

arangodb::Result transactionDataSourceRegistrationCallback(
    arangodb::LogicalDataSource& dataSource,
    arangodb::transaction::Methods& trx
) {
  if (arangodb::iresearch::DATA_SOURCE_TYPE != dataSource.type()) {
    return {}; // not an IResearchView (noop)
  }

  // TODO FIXME find a better way to look up a LogicalView
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* view = dynamic_cast<arangodb::LogicalView*>(&dataSource);
  #else
    auto* view = static_cast<arangodb::LogicalView*>(&dataSource);
  #endif

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to get LogicalView while processing a TransactionState by IResearchFeature for name '" << dataSource.name() << "'";

    return {TRI_ERROR_INTERNAL};
  }

  // TODO FIXME find a better way to look up an IResearch View
  auto& impl = arangodb::LogicalView::cast<arangodb::iresearch::IResearchView>(*view);

  return arangodb::Result(
    impl.apply(trx) ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL
  );
}

void registerTransactionDataSourceRegistrationCallback() {
  if (arangodb::ServerState::instance()->isSingleServer()) {
    arangodb::transaction::Methods::addDataSourceRegistrationCallback(
      &transactionDataSourceRegistrationCallback
    );
  }
}

std::string const FEATURE_NAME("ArangoSearch");
IResearchLogTopic LIBIRESEARCH("libiresearch");

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

bool isFilter(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyFilterFunc;
}

bool isScorer(arangodb::aql::Function const& func) noexcept {
  return func.implementation == &dummyScorerFunc;
}

class IResearchFeature::Async {
 public:
  typedef std::function<bool(size_t& timeoutMsec, bool timeout)> Fn;

  explicit Async(size_t poolSize = 0);
  Async(size_t poolSize, Async&& other);
  ~Async();

  void emplace(std::shared_ptr<ResourceMutex> const& mutex, Fn &&fn); // add an asynchronous task
  void notify() const; // notify all tasks
  size_t poolSize() { return _pool.size(); }
  void start();

 private:
  struct Pending {
    Fn _fn; // the function to execute
    std::shared_ptr<ResourceMutex> _mutex; // mutex for the task resources
    std::chrono::system_clock::time_point _timeout; // when the task should be notified (std::chrono::milliseconds::max() == disabled)

    Pending(std::shared_ptr<ResourceMutex> const& mutex,Fn &&fn)
      : _fn(std::move(fn)),
        _mutex(mutex),
        _timeout(std::chrono::system_clock::time_point::max()) {
    }
  };

  struct Task: public Pending {
    std::unique_lock<ReadMutex> _lock; // prevent resource deallocation

    Task(Pending&& pending): Pending(std::move(pending)) {}
  };

  struct Thread: public arangodb::Thread {
    mutable std::condition_variable _cond; // trigger task run
    mutable std::mutex _mutex; // mutex used with '_cond' and '_pending'
    Thread* _next; // next thread in circular-list (never null!!!) (need to store pointer for move-assignment)
    std::vector<Pending> _pending; // pending tasks
    std::atomic<size_t> _size; // approximate size of the active+pending task list
    std::vector<Task> _tasks; // the tasks to perform
    std::atomic<bool>* _terminate; // trigger termination of this thread (need to store pointer for move-assignment)
    mutable bool _wasNotified; // a notification was raised from another thread

    Thread(std::string const& name)
      : arangodb::Thread(name), _next(nullptr), _terminate(nullptr), _wasNotified(false) {
    }
    Thread(Thread&& other) // used in constructor before tasks are started
      : arangodb::Thread(other.name()), _next(nullptr), _terminate(nullptr), _wasNotified(false) {
    }
    virtual bool isSystem() override { return true; } // or start(...) will fail
    virtual void run() override;
  };

  arangodb::basics::ConditionVariable _join; // mutex to join on
  std::vector<Thread> _pool; // thread pool (size fixed for the entire life of object)
  std::atomic<bool> _terminate; // unconditionaly terminate async tasks

  void stop(Thread* redelegate = nullptr);
};

void IResearchFeature::Async::Thread::run() {
  std::vector<Pending> pendingRedelegate;
  std::chrono::system_clock::time_point timeout;
  bool timeoutSet = false;

  for (;;) {
    bool onlyPending;
    auto pendingStart = _tasks.size();

    {
      SCOPED_LOCK_NAMED(_mutex, lock); // aquire before '_terminate' check so that don't miss notify()

      if (_terminate->load()) {
        break; // termination requested
      }

      // transfer any new pending tasks into active tasks
      for (auto& pending: _pending) {
        _tasks.emplace_back(std::move(pending)); // will aquire resource lock

        auto& task = _tasks.back();

        if (task._mutex) {
          task._lock =
            std::unique_lock<ReadMutex>(task._mutex->mutex(), std::try_to_lock);

          if (!task._lock.owns_lock()) {
            // if can't lock 'task._mutex' then reasign the task to the next worker
            pendingRedelegate.emplace_back(std::move(task));
          } else if (*(task._mutex)) {
            continue; // resourceMutex acquisition successful
          }

          _tasks.pop_back(); // resource no longer valid
        }
      }

      _pending.clear();
      _size.store(_tasks.size());

      // do not sleep if a notification was raised or pending tasks were added
      if (_wasNotified
          || pendingStart < _tasks.size()
          || !pendingRedelegate.empty()) {
        timeout = std::chrono::system_clock::now();
        timeoutSet = true;
      }

      // sleep until timeout
      if (!timeoutSet) {
        _cond.wait(lock); // wait forever
      } else {
        _cond.wait_until(lock, timeout); // wait for timeout or notify
      }

      onlyPending = !_wasNotified && pendingStart < _tasks.size(); // process all tasks if a notification was raised
      _wasNotified = false; // ignore notification since woke up

      if (_terminate->load()) { // check again after sleep
        break; // termination requested
      }
    }

    timeoutSet = false;

    // transfer some tasks to '_next' if have too many
    if (!pendingRedelegate.empty()
        || (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1)) {
      SCOPED_LOCK(_next->_mutex);

      // reasign to '_next' tasks that failed resourceMutex aquisition
      while (!pendingRedelegate.empty()) {
        _next->_pending.emplace_back(std::move(pendingRedelegate.back()));
        pendingRedelegate.pop_back();
        ++_next->_size;
      }

      // transfer some tasks to '_next' if have too many
      while (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1) {
        _next->_pending.emplace_back(std::move(_tasks.back()));
        _tasks.pop_back();
        ++_next->_size;
        --_size;
      }

      _next->_cond.notify_all(); // notify thread about a new task (thread may be sleeping indefinitely)
    }

    for (size_t i = onlyPending ? pendingStart : 0, count = _tasks.size(); // optimization to skip previously run tasks if a notificationw as not raised
         i < count;
        ) {
      auto& task = _tasks[i];
      auto exec = std::chrono::system_clock::now() >= task._timeout;
      size_t timeoutMsec = 0; // by default reschedule for the same time span

      try {
        if (!task._fn(timeoutMsec, exec)) {
          if (i + 1 < count) {
            std::swap(task, _tasks[count - 1]); // swap 'i' with tail
          }

          _tasks.pop_back(); // remove stale tail
          --count;

          continue;
        }
      } catch(...) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "caught error while executing asynchronous task";
        IR_LOG_EXCEPTION();
        timeoutMsec = 0; // sleep until previously set timeout
      }

      // task reschedule time modification requested
      if (timeoutMsec) {
        task._timeout = std::chrono::system_clock::now()
                      + std::chrono::milliseconds(timeoutMsec);
      }

      timeout = timeoutSet ? std::min(timeout, task._timeout) : task._timeout;
      timeoutSet = true;
      ++i;
    }
  }

  // ...........................................................................
  // move all tasks back into _pending in case the may neeed to be reasigned
  // ...........................................................................
  SCOPED_LOCK_NAMED(_mutex, lock); // '_pending' may be modified asynchronously

  for (auto& task: pendingRedelegate) {
    _pending.emplace_back(std::move(task));
  }

  for (auto& task: _tasks) {
    _pending.emplace_back(std::move(task));
  }

  _tasks.clear();
}

IResearchFeature::Async::Async(size_t poolSize): _terminate(false) {
  poolSize = std::max(size_t(1), poolSize); // need at least one thread

  for (size_t i = 0; i < poolSize; ++i) {
    _pool.emplace_back(std::string("ArangoSearch #") + std::to_string(i));
  }

  auto* last = &(_pool.back());

  // build circular list
  for (auto& thread: _pool) {
    last->_next = &thread;
    last = &thread;
    thread._terminate = &_terminate;
  }
}

IResearchFeature::Async::Async(size_t poolSize, Async&& other)
  : Async(poolSize) {
  other.stop(&_pool[0]);
}

IResearchFeature::Async::~Async() {
  stop();
}

void IResearchFeature::Async::emplace(
    std::shared_ptr<ResourceMutex> const& mutex,
    Fn &&fn
) {
  if (!fn) {
    return; // skip empty functers
  }

  auto& thread = _pool[0];
  SCOPED_LOCK(thread._mutex);
  thread._pending.emplace_back(mutex, std::move(fn));
  ++thread._size;
  thread._cond.notify_all(); // notify thread about a new task (thread may be sleeping indefinitely)
}

void IResearchFeature::Async::notify() const {
  // notify all threads
  for (auto& thread: _pool) {
    SCOPED_LOCK(thread._mutex);
    thread._cond.notify_all();
    thread._wasNotified = true;
  }
}

void IResearchFeature::Async::start() {
  // start threads
  for (auto& thread: _pool) {
    thread.start(&_join);
  }

  LOG_TOPIC(DEBUG, arangodb::iresearch::TOPIC)
    << "started " << _pool.size() << " ArangoSearch maintenance thread(s)";
}

void IResearchFeature::Async::stop(Thread* redelegate /*= nullptr*/) {
  _terminate.store(true); // request stop asynchronous tasks
  notify(); // notify all threads

  CONDITION_LOCKER(lock, _join);

  // join with all threads in pool
  for (auto& thread: _pool) {
    if (thread.hasStarted()) {
      while(thread.isRunning()) {
        _join.wait();
      }
    }

    // redelegate all thread tasks if requested
    if (redelegate) {
      SCOPED_LOCK(redelegate->_mutex);

      for (auto& task: thread._pending) {
        redelegate->_pending.emplace_back(std::move(task));
        ++redelegate->_size;
      }

      thread._pending.clear();
      redelegate->_cond.notify_all(); // notify thread about a new task (thread may be sleeping indefinitely)
    }
  }
}

IResearchFeature::IResearchFeature(
    arangodb::application_features::ApplicationServer& server
)
  : ApplicationFeature(server, IResearchFeature::name()),
    _async(std::make_unique<Async>()),
    _running(false),
    _threads(0),
    _threadsLimit(0) {
  setOptional(true);
  startsAfter("V8Phase");
  startsAfter("IResearchAnalyzer"); // used for retrieving IResearch analyzers for functions
  startsAfter("AQLFunctions");
}

void IResearchFeature::async(
    std::shared_ptr<ResourceMutex> const& mutex,
    Async::Fn &&fn
) {
  _async->emplace(mutex, std::move(fn));
}

void IResearchFeature::asyncNotify() const {
  _async->notify();
}

void IResearchFeature::beginShutdown() {
  _running.store(false);
  ApplicationFeature::beginShutdown();
}

void IResearchFeature::collectOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  auto section = FEATURE_NAME;

  _running.store(false);
  std::transform(section.begin(), section.end(), section.begin(), ::tolower);
  ApplicationFeature::collectOptions(options);
  options->addSection(section, std::string("Configure the ") + FEATURE_NAME + " feature");
  options->addOption(
    std::string("--") + section + ".threads",
    "the exact number of threads to use for asynchronous tasks (0 == autodetect)",
    new arangodb::options::UInt64Parameter(&_threads)
  );
  options->addOption(
    std::string("--") + section + ".threads-limit",
    "upper limit to the autodetected number of threads to use for asynchronous tasks (0 == use default)",
    new arangodb::options::UInt64Parameter(&_threadsLimit)
  );
}

/*static*/ std::string const& IResearchFeature::name() {
  return FEATURE_NAME;
}

void IResearchFeature::prepare() {
  if (!isEnabled()) {
    return;
  }

  _running.store(false);
  ApplicationFeature::prepare();

  // load all known codecs
  ::iresearch::formats::init();

  // load all known scorers
  ::iresearch::scorers::init();

  // register 'arangosearch' index
  registerIndexFactory();

  // register 'arangosearch' view
  registerViewFactory();

  // register 'arangosearch' Transaction DataSource registration callback
  registerTransactionDataSourceRegistrationCallback();

  registerRecoveryHelper();

  // start the async task thread pool
  if (!ServerState::instance()->isCoordinator() &&
      !ServerState::instance()->isAgent()) {
    auto poolSize = computeThreadPoolSize(_threads, _threadsLimit);

    if (_async->poolSize() != poolSize) {
      _async = std::make_unique<Async>(poolSize, std::move(*_async));
    }

    _async->start();
  }
}

void IResearchFeature::start() {
  if (!isEnabled()) {
    return;
  }

  ApplicationFeature::start();

  // register IResearchView filters
  {
    auto* functions = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::aql::AqlFunctionFeature
    >("AQLFunctions");

    if (functions) {
      registerFilters(*functions);
      registerScorers(*functions);
      registerFunctions(*functions);
    } else {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'AQLFunctions' while registering arangosearch filters";
    }

  }

  _running.store(true);
}

void IResearchFeature::stop() {
  if (!isEnabled()) {
    return;
  }
  _running.store(false);
  ApplicationFeature::stop();
}

void IResearchFeature::unprepare() {
  if (!isEnabled()) {
    return;
  }

  _running.store(false);
  ApplicationFeature::unprepare();
}

void IResearchFeature::validateOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running.store(false);
  ApplicationFeature::validateOptions(options);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
