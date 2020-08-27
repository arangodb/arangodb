////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "test_main.h"
#include <fuerte/fuerte.h>
#include <fuerte/loop.h>
#include <fuerte/helper.h>

#include <velocypack/Parser.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <iostream>
#include <fstream>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <thread>

namespace f = ::arangodb::fuerte;

class Connection100kWritesF : public ::testing::Test {
 protected:
  Connection100kWritesF(){
    _server = "vst://127.0.0.1:8529";
  }
  virtual ~Connection100kWritesF() noexcept {}

  virtual void SetUp() override {
    try {
      f::ConnectionBuilder cbuilder;
      cbuilder.host(_server);
      _connection = cbuilder.connect(_eventLoopService);

      //delete collection
      {
        auto request = arangodb::fuerte::createRequest(
          arangodb::fuerte::RestVerb::Delete, "/_api/collection/testobi");
        auto result = _connection->sendRequest(std::move(request));
        //arangodb::fuerte::run();
      }
      //create collection
      {
        namespace fu = arangodb::fuerte;
        VPackBuilder builder;
        builder.openObject();
        builder.add("name" , VPackValue("testobi"));
        builder.close();
        arangodb::fuerte::Request request = *fu::createRequest(fu::RestVerb::Post, "/_api/collection");
        request.addVPack(builder.slice());
        auto result = _connection->sendRequest(std::move(request));
        if (result->header.responseCode >= 400){
          std::cerr << fu::to_string(request);
          std::cerr << fu::to_string(*result);
          ASSERT_TRUE(false);
          throw;
        }
      }
    } catch(std::exception const& ex) {
      std::cout << "SETUP OF FIXTURE FAILED" << std::endl;
      throw ex;
    }
  }

  virtual void TearDown() override {
    //delete collection
    {
      auto request = arangodb::fuerte::createRequest(
        arangodb::fuerte::RestVerb::Delete, "/_api/collection/testobi");
      auto result = _connection->sendRequest(std::move(request));
      //arangodb::fuerte::run();
    }
    _connection.reset();
  }

  std::shared_ptr<f::Connection> _connection;

 private:
  f::EventLoopService _eventLoopService;
  std::string _server;
  std::string _port;
};

namespace fu = ::arangodb::fuerte;

/*
TEST_F(Connection100kWritesF, Writes100k){
  bool use_threads = true;
  bool debug = false;

  std::ifstream fh("bodies-full.json");
  ASSERT_TRUE(fh.is_open());
  std::stringstream ss;
  ss << fh.rdbuf();
  auto content = ss.str();

  VPackParser parser;
  parser.parse(content);
  auto builder = parser.steal();

  fu::OnErrorCallback onError = [](fu::Error error, std::unique_ptr<fu::Request> req, std::unique_ptr<fu::Response> res) {
    ASSERT_TRUE(false) << fu::to_string(fu::intToError(error));
    std::cerr << fu::to_string(fu::intToError(error));
    assert(false);
    throw;
  };

  fu::OnSuccessCallback onSuccess = [](std::unique_ptr<fu::Request> req, std::unique_ptr<fu::Response> res){
    assert(req);
    assert(res);
    //ASSERT_TRUE(res->header.responseCode < 400);
    if (res->header.responseCode >= 400) {
      std::cerr << res->messageid << std::endl;
      std::cerr << fu::to_string(*req);
      std::cerr << fu::to_string(*res);
      assert(false);
      ASSERT_TRUE(false);
      throw;
    }
    std::cerr << res->messageid << "ok!" << std::endl;

  };

	size_t numThreads = std::thread::hardware_concurrency();
	boost::thread_group     threads;
	boost::barrier          barrier(numThreads);
	auto asioLoop = fu::getProvider().getAsioLoop();
	auto work = std::make_shared<asio_ns::io_service::work>(*asioLoop->getIoService());

  if (use_threads) {
		for( unsigned int i = 0; i < numThreads; ++i ){
				threads.create_thread( [&]() {
					while (work){
							try {
								asioLoop->direct_run();
							} catch (std::exception const& e){
								std::cout << e.what();
								if( barrier.wait() ) {
									asioLoop->direct_reset();
								}
							}
					}
				});
		}
  } else {
		work.reset();
	}

  if(debug){
    std::cerr << "enter loop" << std::endl;
  }
  std::size_t i = 0;
  for(auto const& slice : VPackArrayIterator(builder->slice())){
    ++i;
    if (!use_threads && i % 50 == 0){
      fu::run();
    }
    auto request = fu::createRequest(fu::RestVerb::Post, "/_api/document/testobi");
    request->addBinary(slice.start(),slice.byteSize());
    _connection->sendRequest(std::move(request), onError, onSuccess);
  }

  if (!use_threads){
    fu::run();
  } else {
		work.reset();
		// //find out how much work is left !! //check sendQueue and map
		// asioLoop->direct_stop();
		// threads.interrupt_all();
		threads.join_all();
	}
}

*/