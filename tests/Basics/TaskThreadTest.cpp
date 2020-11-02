////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Basics/Thread.h"
#include "Logger/Logger.h"
#include "Mocks/LogLevels.h"

#include "gtest/gtest.h"

using namespace arangodb;

TEST(TaskThreadTest, testCreate) {
  class Testee : public TaskThread {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    bool runTask() override {
      return false;
    }
  };
  application_features::ApplicationServer server(nullptr, nullptr);

  Testee t(server);

  ASSERT_EQ("testee", t.name());
  ASSERT_FALSE(t.isSystem());
  ASSERT_FALSE(t.isSilent());
}

TEST(TaskThreadTest, testRunTask) {
  class Testee : public TaskThread {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    bool runTask() override {
      return (++_counter != 1000);
    }

    int counter() const { return _counter; }

   private:
    int _counter = 0;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  ASSERT_EQ(0, t.counter());
  t.run();

  ASSERT_EQ(1000, t.counter());
}

TEST(TaskThreadTest, testRunTaskWithException) {
  class Testee : public TaskThread,
                 public arangodb::tests::LogSuppressor<arangodb::Logger::THREADS, arangodb::LogLevel::FATAL> {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    bool runTask() override {
      if (++_counter == 10) {
        return false;
      }

      // intentionally failing!
      throw "peng!";
    }

    int counter() const { return _counter; }

   private:
    int _counter = 0;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  ASSERT_EQ(0, t.counter());
  t.run(); // run() must catch the exception for us

  ASSERT_EQ(10, t.counter());
}

TEST(TaskThreadTest, testRunSetup) {
  class Testee : public TaskThread {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    void runSetup() override { _setup = true; }

    bool runTask() override {
      return ++_counter != 10;
    }

    bool setup() const { return _setup; }
    int counter() const { return _counter; }

   private:
    bool _setup = false;
    int _counter = 0;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  ASSERT_EQ(0, t.counter());
  ASSERT_FALSE(t.setup());
  t.run(); 

  ASSERT_TRUE(t.setup());
  ASSERT_EQ(10, t.counter());
}

TEST(TaskThreadTest, testRunSetupWithException) {
  class Testee : public TaskThread {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    void runSetup() override { 
      throw std::string("peng!");
    }

    bool runTask() override {
      _ran = true;
      return true;
    }

    bool ran() const { return _ran; }

   private:
    bool _ran = false;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  try {
    ASSERT_FALSE(t.ran());
    t.run(); // run() will fail during setup and not catch the exception

    ASSERT_FALSE(true);
  } catch (std::string const& ex) {
    ASSERT_EQ(ex, "peng!");
  }

  ASSERT_FALSE(t.ran());
}

TEST(TaskThreadTest, testRunTeardown) {
  class Testee : public TaskThread {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    bool runTask() override {
      return ++_counter != 10;
    }

    void runTeardown() override { _teardown = true; }

    int counter() const { return _counter; }
    bool teardown() const { return _teardown; }

   private:
    bool _teardown = false;
    int _counter = 0;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  ASSERT_EQ(0, t.counter());
  ASSERT_FALSE(t.teardown());
  t.run(); 

  ASSERT_EQ(10, t.counter());
  ASSERT_TRUE(t.teardown());
}

TEST(TaskThreadTest, testTeardownWithException) {
  class Testee : public TaskThread,
                 public arangodb::tests::LogSuppressor<arangodb::Logger::THREADS, arangodb::LogLevel::FATAL> {
   public:
    Testee(application_features::ApplicationServer& server)
        : TaskThread(server, "testee") {}
    ~Testee() { shutdown(); }

    bool runTask() override {
      return false;
    }
    
    void runTeardown() override {
      _teardown = true;
      throw std::string("peng!");
    }

    bool teardown() const { return _teardown; }

   private:
    bool _teardown = false;
  };

  application_features::ApplicationServer server(nullptr, nullptr);
  Testee t(server);

  ASSERT_FALSE(t.teardown());
  t.run(); // run() will catch the exception during teardown
  
  ASSERT_TRUE(t.teardown());
}
