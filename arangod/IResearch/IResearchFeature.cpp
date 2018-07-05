
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

arangodb::aql::AqlValue filter(
    arangodb::aql::Query*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "Filter function is designed to be used with ArangoSearch view only"
  );
}

arangodb::aql::AqlValue scorer(
    arangodb::aql::Query*,
    arangodb::transaction::Methods* ,
    arangodb::SmallVector<arangodb::aql::AqlValue> const&) {
  THROW_ARANGO_EXCEPTION_MESSAGE(
    TRI_ERROR_NOT_IMPLEMENTED,
    "Scorer function is designed to be used with ArangoSearch view only"
  );
}

typedef arangodb::aql::AqlValue (*IResearchFunctionPtr)(
  arangodb::aql::Query*,
  arangodb::transaction::Methods* ,
  arangodb::SmallVector<arangodb::aql::AqlValue> const&
);

void registerFunctions(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(functions, {
    "__ARANGOSEARCH_SCORE_DEBUG",  // name
    ".",    // value to convert
    true,   // deterministic
    false,  // can't throw
    true,   // can be run on server
    [](arangodb::aql::Query*,
       arangodb::transaction::Methods*,
       arangodb::SmallVector<arangodb::aql::AqlValue> const& args) noexcept {
      auto arg = arangodb::aql::Functions::ExtractFunctionParameterValue(args, 0);
      auto const floatValue = *reinterpret_cast<float_t const*>(arg.slice().begin());
      return arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble(double_t(floatValue)));
    }
  });
}

void registerFilters(arangodb::aql::AqlFunctionFeature& functions) {
  arangodb::iresearch::addFunction(functions, {
    "EXISTS",      // name
    ".|.,.",         // positional arguments (attribute, [ "analyzer"|"type"|"string"|"numeric"|"bool"|"null" ])
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "STARTS_WITH", // name
    ".,.|.",       // positional arguments (attribute, prefix, scoring-limit)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "PHRASE",      // name
    ".,.|.+",      // positional arguments (attribute, input [, offset, input... ] [, analyzer])
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "MIN_MATCH",   // name
    ".,.|.+",      // positional arguments (filter expression [, filter expression, ... ], min match count)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "BOOST",       // name
    ".,.",         // positional arguments (filter expression, boost)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });

  arangodb::iresearch::addFunction(functions, {
    "ANALYZER",    // name
    ".,.",         // positional arguments (filter expression, analyzer)
    true,          // deterministic
    true,          // can throw
    true,          // can be run on server
    &filter        // function implementation (use function name as placeholder)
  });
}

void registerIndexFactory() {
  static const std::map<std::string, arangodb::IndexFactory::IndexTypeFactory> factories = {
    { "ClusterEngine", arangodb::iresearch::IResearchLinkCoordinator::make },
    { "MMFilesEngine", arangodb::iresearch::IResearchMMFilesLink::make },
    { "RocksDBEngine", arangodb::iresearch::IResearchRocksDBLink::make }
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
    auto res = indexFactory.emplaceFactory(indexType, entry.second);

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering IResearch link factory with index factory from feature '") + entry.first + "': " + res.errorMessage()
      );
    }

    res = indexFactory.emplaceNormalizer(
      indexType, arangodb::iresearch::IResearchLinkHelper::normalize
    );

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res.errorNumber(),
        std::string("failure registering IResearch link normalizer with index factory from feature '") + entry.first + "': " + res.errorMessage()
      );
    }
  }
}

void registerScorers(arangodb::aql::AqlFunctionFeature& functions) {
  irs::scorers::visit([&functions](
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
      ".|+", // positional arguments (attribute [, <scorer-specific properties>...])
      true,   // deterministic
      false,  // can't throw
      true,   // can be run on server
      &scorer // function implementation (use function name as placeholder)
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
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewCoordinator::make);
  } else if (arangodb::ServerState::instance()->isDBServer()) {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchViewDBServer::make);
  } else {
    res = viewTypes->emplace(viewType, arangodb::iresearch::IResearchView::make);
  }

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      res.errorNumber(),
      std::string("failure registering IResearch view factory: ") + res.errorMessage()
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
  auto* pimpl = func.implementation.target<IResearchFunctionPtr>();
  return pimpl && *pimpl == &filter;
}

bool isScorer(arangodb::aql::Function const& func) noexcept {
  auto* pimpl = func.implementation.target<IResearchFunctionPtr>();
  return pimpl && *pimpl == &scorer;
}

class IResearchFeature::Async {
 public:
  typedef std::function<bool(size_t& timeoutMsec, bool timeout)> Fn;

  Async();
  ~Async();

  void emplace(
    std::shared_ptr<ResourceMutex> const& mutex,
    size_t timeoutMsec,
    Fn &&fn
  ); // add an asynchronous tasks
  void notify() const; // notify all tasks

 private:
  struct Pending {
    Fn _fn; // the function to execute
    std::shared_ptr<ResourceMutex> _mutex; // mutex for the task resources
    std::chrono::system_clock::time_point _timeout; // when the task should be notified (std::chrono::milliseconds::max() == disabled)

    Pending(
        std::shared_ptr<ResourceMutex> const& mutex,
        size_t timeoutMsec,
        Fn &&fn
    ): _fn(std::move(fn)),
       _mutex(mutex),
       _timeout(
         timeoutMsec
         ? (std::chrono::system_clock::now() + std::chrono::milliseconds(timeoutMsec))
         : std::chrono::system_clock::time_point::max()
       ) {
    }
  };

  struct Task: public Pending {
    std::unique_lock<ReadMutex> _lock; // prevent resource deallocation

    Task(Pending&& pending): Pending(std::move(pending)) {
      // lock resource mutex or ignore if none supplied
      if(_mutex) {
        _lock = std::unique_lock<ReadMutex>(_mutex->mutex());
      }
    }
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
      : arangodb::Thread(name), _wasNotified(false) {
    }
    Thread(Thread&& other): arangodb::Thread(other.name()) {} // used in constructor before tasks are started
    virtual bool isSystem() override { return true; } // or start(...) will fail
    virtual void run() override;
  };

  arangodb::basics::ConditionVariable _join; // mutex to join on
  std::vector<Thread> _pool; // thread pool (size fixed for the entire life of object)
  std::atomic<bool> _terminate; // unconditionaly terminate async tasks
};

void IResearchFeature::Async::Thread::run() {
  std::chrono::system_clock::time_point timeout;
  bool timeoutSet = false;

  for (;;) {
    {
      SCOPED_LOCK_NAMED(_mutex, lock); // aquire before '_terminate' check so that don't miss notify()

      if (_terminate->load()) {
        return; // termination requested
      }

      // transfer any new pending tasks into active tasks
      for (auto& pending: _pending) {
        _tasks.emplace_back(std::move(pending)); // will aquire resource lock

        auto& task = _tasks.back();

        if (task._mutex && !*(task._mutex)) {
          _tasks.pop_back(); // resource no longer valid
          continue;
        }

        if (std::chrono::system_clock::time_point::max() != task._timeout) {
          timeout =
            timeoutSet ? std::min(timeout, task._timeout) : task._timeout;
          timeoutSet = true;
        }
      }

      _pending.clear();
      _size.store(_tasks.size());

      // do not sleep if a notification was raised
      if (_wasNotified) {
        timeout = std::chrono::system_clock::now();
        timeoutSet = true;
      }

      // sleep until timeout
      if (!timeoutSet) {
        _cond.wait(lock); // wait forever
      } else {
        _cond.wait_until(lock, timeout); // wait for timeout or notify
      }

      _wasNotified = false; // ignore notification since woke up

      if (_terminate->load()) { // check again after sleep
        return; // termination requested
      }
    }

    timeoutSet = false;

    // transfer some tasks to '_next' if have too many
    while (_size.load() > _next->_size.load() * 2 && _tasks.size() > 1) {
      SCOPED_LOCK(_next->_mutex);
      _next->_pending.emplace_back(std::move(_tasks.back()));
      _tasks.pop_back();
      ++_next->_size;
      --_size;
      _next->_cond.notify_all(); // notify thread about a new task (thread may be sleeping indefinitely)
      _next->_wasNotified = true; // ensure the next thread checks task validity since task was supposed to be checked in this thread
    }

    for (size_t i = 0, count = _tasks.size(); i < count;) {
      auto& task = _tasks[i];
      auto exec = std::chrono::system_clock::now() >= task._timeout;
      size_t timeoutMsec;

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
}

IResearchFeature::Async::Async(): _terminate(false) {
  static const unsigned int MAX_THREADS = 8; // arbitrary limit on the upper bound of threads in pool
  static const unsigned int MIN_THREADS = 1; // at least one thread is required
  auto poolSize = std::max(
    MIN_THREADS,
    std::min(MAX_THREADS, std::thread::hardware_concurrency())
  );

  for (size_t i = 0; i < poolSize; ++i) {
    _pool.emplace_back(std::string("ArangoSearch #") + std::to_string(i));
  }

  auto* last = &(_pool.back());

  // buld circular list and start threads
  for (auto& thread: _pool) {
    last->_next = &thread;
    last = &thread;
    thread._terminate = &_terminate;
    thread.start(&_join);
  }
}

IResearchFeature::Async::~Async() {
  _terminate.store(true); // request stop asynchronous tasks
  notify(); // notify all threads

  CONDITION_LOCKER(lock, _join);

  // join with all threads in pool
  for (auto& thread: _pool) {
    while(thread.isRunning()) {
      _join.wait();
    }
  }
}

void IResearchFeature::Async::emplace(
    std::shared_ptr<ResourceMutex> const& mutex,
    size_t timeoutMsec,
    Fn &&fn
) {
  if (!fn) {
    return; // skip empty functers
  }

  auto& thread = _pool[0];
  SCOPED_LOCK(thread._mutex);
  thread._pending.emplace_back(mutex, timeoutMsec, std::move(fn));
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

IResearchFeature::IResearchFeature(arangodb::application_features::ApplicationServer* server)
  : ApplicationFeature(server, IResearchFeature::name()),
    _async(std::make_unique<Async>()),
    _running(false) {
  setOptional(true);
  startsAfter("ViewTypes");
  startsAfter("Logger");
  startsAfter("Database");
  startsAfter("IResearchAnalyzer"); // used for retrieving IResearch analyzers for functions
  startsAfter("AQLFunctions");
  // TODO FIXME: we need the MMFilesLogfileManager to be available here if we
  // use the MMFiles engine. But it does not feel right to have such storage engine-
  // specific dependency here. Better create a "StorageEngineFeature" and make
  // ourselves start after it!
  startsAfter("MMFilesLogfileManager");
  startsAfter("TransactionManager");

  startsBefore("GeneralServer");
}

void IResearchFeature::async(
    std::shared_ptr<ResourceMutex> const& mutex,
    size_t timeoutMsec,
    Async::Fn &&fn
) {
  _async->emplace(mutex, timeoutMsec, std::move(fn));
}

void IResearchFeature::asyncNotify() const {
  _async->notify();
}

void IResearchFeature::beginShutdown() {
  _running = false;
  ApplicationFeature::beginShutdown();
}

void IResearchFeature::collectOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::collectOptions(options);
}

/*static*/ std::string const& IResearchFeature::name() {
  return FEATURE_NAME;
}

void IResearchFeature::prepare() {
  _running = false;
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
}

void IResearchFeature::start() {
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
        << "failure to find feature 'AQLFunctions' while registering iresearch filters";
    }

  }

  _running = true;
}

void IResearchFeature::stop() {
  _running = false;
  ApplicationFeature::stop();
}

void IResearchFeature::unprepare() {
  _running = false;
  ApplicationFeature::unprepare();
}

void IResearchFeature::validateOptions(
    std::shared_ptr<arangodb::options::ProgramOptions> options
) {
  _running = false;
  ApplicationFeature::validateOptions(options);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------