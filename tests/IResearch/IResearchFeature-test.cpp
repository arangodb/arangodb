////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"
#include "common.h"
#include "../Mocks/StorageEngineMock.h"

#include "utils/misc.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/version_defines.hpp"

#include "Aql/AqlFunctionFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchFeature.h"
#include "Rest/Version.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchFeatureSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchFeatureSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
  }

  ~IResearchFeatureSetup() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchFeatureTest", "[iresearch][iresearch-feature]") {
  IResearchFeatureSetup s;
  UNUSED(s);

SECTION("test_start") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* functions = new arangodb::aql::AqlFunctionFeature(server);
  arangodb::iresearch::IResearchFeature iresearch(server);
  auto cleanup = irs::make_finally([functions]()->void{ functions->unprepare(); });

  enum class FunctionType {
    FILTER = 0,
    SCORER
  };

  std::map<irs::string_ref, std::pair<irs::string_ref, FunctionType>> expected = {
    // filter functions
    { "EXISTS", { ".|.,.", FunctionType::FILTER } },
    { "PHRASE", { ".,.|.+", FunctionType::FILTER } },
    { "STARTS_WITH", { ".,.|.", FunctionType::FILTER } },
    { "MIN_MATCH", { ".,.|.+", FunctionType::FILTER } },

    // context functions
    { "ANALYZER", { ".,.", FunctionType::FILTER } },
    { "BOOST", { ".,.", FunctionType::FILTER } },

    // scorer functions
    { "BM25", { ".|+", FunctionType::SCORER } },
    { "TFIDF", { ".|+", FunctionType::SCORER } },
  };

  server.addFeature(functions);
  functions->prepare();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    CHECK((nullptr == function));
  };

  iresearch.start();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    CHECK((nullptr != function));
    CHECK((entry.second.first == function->arguments));
    CHECK((
      entry.second.second == FunctionType::FILTER && arangodb::iresearch::isFilter(*function)
      || entry.second.second == FunctionType::SCORER && arangodb::iresearch::isScorer(*function)
    ));
  };
}

SECTION("IResearch_version") {
  CHECK(IResearch_version == arangodb::rest::Version::getIResearchVersion());
  CHECK(IResearch_version == arangodb::rest::Version::Values["iresearch-version"]);
}

// Temporarily surpress for MSVC
#ifndef _MSC_VER
SECTION("test_async") {
  // schedule task (null resource mutex)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(nullptr, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
  }

  // schedule task (null resource mutex value)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(nullptr);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    CHECK((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
  }

  // schedule task (null functr)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    feature.async(resourceMutex, {});
    CHECK((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    resourceMutex->reset(); // should not deadlock if task released
  }

  // schedule task (wait indefinite)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(nullptr, [&cond, &mutex, flag, &count](size_t&, bool)->bool { ++count; SCOPED_LOCK(mutex); cond.notify_all(); return true; });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    CHECK((false == deallocated));
    CHECK((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    CHECK((false == deallocated)); // still scheduled
    CHECK((1 == count));
  }

  // single-run task
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
  }

  // multi-run task
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    std::chrono::system_clock::duration diff;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool)->bool {
        diff = std::chrono::system_clock::now() - last;
        last = std::chrono::system_clock::now();
        timeoutMsec = 100;
        if (++count <= 1) return true;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        return false;
      });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
    CHECK((2 == count));
    CHECK((std::chrono::milliseconds(100) < diff));
  }

  // trigger task by notify
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    bool execVal = true;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &execVal, &count](size_t&, bool exec)->bool { execVal = exec; SCOPED_LOCK(mutex); cond.notify_all(); return ++count < 2; });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    CHECK((false == deallocated));
    CHECK((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    CHECK((false == deallocated));
    feature.asyncNotify();
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
    CHECK((false == execVal));
    auto diff = std::chrono::system_clock::now() - last;
    CHECK((std::chrono::milliseconds(1000) > diff));
  }

  // trigger by timeout
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    bool execVal = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &execVal, &count](size_t& timeoutMsec, bool exec)->bool {
        execVal = exec;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        timeoutMsec = 100;
        return ++count < 2;
      });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    CHECK((false == deallocated));
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
    CHECK((true == execVal));
    auto diff = std::chrono::system_clock::now() - last;
    CHECK((std::chrono::milliseconds(300) >= diff)); // could be a little more then 100ms+100ms
  }

  // deallocate empty
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);

    {
      arangodb::iresearch::IResearchFeature feature(server);
      server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
      feature.prepare(); // start thread pool
    }
  }

  // deallocate with running tasks
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      arangodb::iresearch::IResearchFeature feature(server);
      server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
      feature.prepare(); // start thread pool
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });

      feature.async(resourceMutex, [&cond, &mutex, flag](size_t& timeoutMsec, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); timeoutMsec = 100; return true; });
      CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    }

    CHECK((true == deallocated));
  }

  // multiple tasks with same resourceMutex + resourceMutex reset (sequential creation)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated0 = false;
    bool deallocated1 = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated0, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count](size_t& timeoutMsec, bool)->bool {
        if (++count > 1) return false;
        timeoutMsec = 100;
        SCOPED_LOCK_NAMED(mutex, lock);
        cond.notify_all();
        cond.wait(lock);
        return true;
      });
    }
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000)))); // wait for the first task to start

    std::thread thread([resourceMutex]()->void { resourceMutex->reset(); }); // try to aquire a write lock
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // hopefully a write-lock aquisition attempt is in progress

    {
      TRY_SCOPED_LOCK_NAMED(resourceMutex->mutex(), resourceLock);
      CHECK((false == resourceLock.owns_lock())); // write-lock aquired successfully (read-locks blocked)
    }

    {
      std::shared_ptr<bool> flag(&deallocated1, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [flag](size_t&, bool)->bool { return false; }); // will never get invoked because resourceMutex is reset
    }
    cond.notify_all(); // wake up first task after resourceMutex write-lock aquired (will process pending tasks)
    lock.unlock(); // allow first task to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated0));
    CHECK((true == deallocated1));
    thread.join();
  }

  // schedule task (resize pool)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    arangodb::options::ProgramOptions options("", "", "", nullptr);
    auto optionsPtr = std::shared_ptr<arangodb::options::ProgramOptions>(&options, [](arangodb::options::ProgramOptions*)->void {});
    feature.collectOptions(optionsPtr);
    options.get<arangodb::options::UInt64Parameter>("arangosearch.threads")->set("8");
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    std::chrono::system_clock::duration diff;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool)->bool {
        diff = std::chrono::system_clock::now() - last;
        last = std::chrono::system_clock::now();
        timeoutMsec = 100;
        if (++count <= 1) return true;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        return false;
      });
    }
    feature.prepare(); // start thread pool after a task has been scheduled, to trigger resize with a task
    CHECK((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK((true == deallocated));
    CHECK((2 == count));
    CHECK((std::chrono::milliseconds(100) < diff));
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
