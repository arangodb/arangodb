#include "curl-stuff.h"
#include <iostream>
#include <latch>

#include "curl/curl.h"

#include "ConnectionPool.h"
#include "Basics/Result.h"
#include "Network/Methods.h"
#include "Network/ConnectionPool.h"

using namespace arangodb;
using namespace arangodb::network;

// calls fn every d intervals for total amount of time
template<typename Duration, typename F>
void rate(Duration d, uint64_t total, F&& fn) {
  using clock = std::chrono::steady_clock;

  uint64_t actual_calls = 0;
  uint64_t expected_calls = 0;
  auto const start = clock::now();
  while (true) {
    auto const now = clock::now();
    expected_calls = (now - start) / d;

    while (expected_calls > actual_calls) {
      fn();
      actual_calls += 1;

      if (actual_calls >= total) {
        return;
      }
    }

    auto next_round = start + (actual_calls + 1) * d;
    std::this_thread::sleep_until(next_round);
  }
}

struct NetworkInterface {
  virtual ~NetworkInterface() = default;

  // to be expanded with additional parameter
  virtual void sendRequest(std::function<void(Result)>) = 0;
};

void send_requests(NetworkInterface& net, std::latch& done, int counter,
                   int& errors) {
  if (counter == 0) {
    done.count_down();
  } else {
    net.sendRequest([&, counter](Result res) {
      if (!res.ok()) {
        done.count_down();
        return;
      }
      send_requests(net, done, counter - 1, errors);
    });
  }
}

void rate_test(NetworkInterface& net, std::ptrdiff_t number_of_requests) {
  std::latch latch(number_of_requests);
  auto const start = std::chrono::steady_clock::now();

  auto errors = 0;
  rate(std::chrono::microseconds{5}, number_of_requests, [&] {
    net.sendRequest([&](Result res) {
      if (!res.ok()) {
        ++errors;
      }

      latch.count_down();
    });
  });

  // std::cout << "done sending\n";
  latch.wait();
  auto const end = std::chrono::steady_clock::now();

  auto const seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
  if (errors == 0) {
    std::cout << "rate_test : " << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  } else {
    std::cout << "rate_test : failed" << std::endl;
  }
}

void thread_test(NetworkInterface& net, std::ptrdiff_t number_of_requests) {
  constexpr auto number_of_threads = 5;
  std::latch latch(number_of_threads);
  auto errors = 0;

  auto const start = std::chrono::steady_clock::now();
  auto num_reqs_to_distribute = number_of_requests;
  for (auto i = 0; i < number_of_threads; i++) {
    auto const reqs = num_reqs_to_distribute / (number_of_threads - i);
    num_reqs_to_distribute -= reqs;
    send_requests(net, latch, reqs, errors);
  }
  latch.wait();
  auto const end = std::chrono::steady_clock::now();

  auto const seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
  if (errors == 0) {
    std::cout << "thread_test : " << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  } else {
    std::cout << "thread_test : failed" << std::endl;
  }
}

struct CurlNetworkInterface final : NetworkInterface {
  CurlNetworkInterface(size_t num, curl::http_version httpVersion)
      : pool(num, httpVersion) {}

  void sendRequest(std::function<void(Result)> function) override {
    curl::send_request(
        pool.next_pool(), curl::http_method::kGet, "http://localhost:8529",
        "http://localhost:8529/_api/version", {}, {},
        [=](curl::response const& response, int code) {
          auto res = [&] {
            if (code != 0) {
              std::cerr << "CODE [" << response.unique_id
                        << "] = " << curl_easy_strerror(CURLcode(code))
                        << std::endl;
              return Result{TRI_ERROR_WAS_ERLAUBE};
            }

            return Result{};
          }();
          function(res);
        });
  }

  curl::multi_connection_pool pool;
};

struct FuerteNetworkInterface final : NetworkInterface {
  void sendRequest(std::function<void(Result)> function) override {
    network::sendRequest(&pool, "http://localhost:8529",
                         arangodb::fuerte::RestVerb::Get, "_api/version", {},
                         {}, {})
        .thenFinal([=](auto&& result) {
          auto res = result.get().combinedResult();
          if (!res.ok()) {
            std::cerr << "FUERTE ERROR " << res.errorMessage() << std::endl;
          }
          function(res);
        });
  }

  FuerteNetworkInterface()
      : pool(network::ConnectionPool::Config{
            .metrics = network::ConnectionPool::Metrics::createStub("fuerte"),
            .maxOpenConnections = 10000,
            .numIOThreads = 1,
            .verifyHosts = false,
            .protocol = arangodb::fuerte::ProtocolType::Http,
        }) {}

  network::ConnectionPool pool;
};

int main(int argc, char* argv[]) {
  {
    std::cout << "CURL HTTP 1" << std::endl;
    CurlNetworkInterface net{4, arangodb::network::curl::http_version::http1};

    auto number_of_requests = 10000;
    rate_test(net, number_of_requests);
    // rate_test(number_of_requests, curl::http_version::http2);
    number_of_requests = 1000;
    thread_test(net, number_of_requests);
    // thread_test(number_of_requests, curl::http_version::http2);
  }
  {
    std::cout << "FUERTE HTTP 1" << std::endl;
    FuerteNetworkInterface net;

    auto number_of_requests = 10000;
    rate_test(net, number_of_requests);
    // rate_test(number_of_requests, curl::http_version::http2);
    number_of_requests = 1000;
    thread_test(net, number_of_requests);
    // thread_test(number_of_requests, curl::http_version::http2);
  }
}
