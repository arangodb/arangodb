#include "curl-stuff.h"
#include <iostream>
#include <latch>

#include "curl/curl.h"

#include "ConnectionPool.h"

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

void send_requests(curl::multi_connection_pool& pool, std::latch& done, int counter,
                   int& errors) {
  if (counter == 0) {
    done.count_down();
  } else {
    curl::request_options options;
    curl::send_request(
        pool.next_pool(), arangodb::network::curl::http_method::kGet,
        "http://localhost:8529", "http://localhost:8529/_api/version", {},
        options, [&, counter](curl::response const& response, int code) {
          if (code != 0 && errors < 3) {
            ++errors;
            std::cerr << "CODE = " << curl_easy_strerror(CURLcode(code))
                      << std::endl;
          }
          if (errors > 0) {
            done.count_down();
            return;
          }
          send_requests(pool, done, counter - 1, errors);
        });
  }
}

void rate_test(std::ptrdiff_t number_of_requests,
               curl::http_version httpVersion) {
  curl::multi_connection_pool pools(4, httpVersion);

  std::latch latch(number_of_requests);
  auto const start = std::chrono::steady_clock::now();

  auto errors = 0;
  rate(std::chrono::microseconds{5}, number_of_requests, [&] {
    curl::request_options options;
    curl::send_request(
        pools.next_pool(), arangodb::network::curl::http_method::kGet,
        "http://localhost:8529", "http://localhost:8529/_api/version", {},
        options, [&](curl::response const& response, int code) {
          if (code != 0 && errors < 3) {
            std::cerr << "CODE [" << response.unique_id << "] = " << curl_easy_strerror(CURLcode(code))
                      << std::endl;
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
    std::cout << "rate_test ("
              << (httpVersion == curl::http_version::http1 ? "http1" : "http2")
              << "): " << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  } else {
    std::cout << "rate_test ("
              << (httpVersion == curl::http_version::http1 ? "http1" : "http2")
              << "): failed" << std::endl;
  }
}

void thread_test(std::ptrdiff_t number_of_requests,
                 curl::http_version httpVersion) {
  constexpr auto number_of_threads = 5;
  curl::multi_connection_pool pool(4, httpVersion);
  std::latch latch(number_of_threads);
  auto errors = 0;

  auto const start = std::chrono::steady_clock::now();
  auto num_reqs_to_distribute = number_of_requests;
  for (auto i = 0; i < number_of_threads; i++) {
    auto const reqs = num_reqs_to_distribute / (number_of_threads - i);
    num_reqs_to_distribute -= reqs;
    send_requests(pool, latch, reqs, errors);
  }
  latch.wait();
  auto const end = std::chrono::steady_clock::now();

  auto const seconds =
      std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
  if (errors == 0) {
    std::cout << "thread_test ("
              << (httpVersion == curl::http_version::http1 ? "http1" : "http2")
              << "): " << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  } else {
    std::cout << "thread_test ("
              << (httpVersion == curl::http_version::http1 ? "http1" : "http2")
              << "): failed" << std::endl;
  }
}

int main(int argc, char* argv[]) {
  auto number_of_requests = 10000;
  rate_test(number_of_requests, curl::http_version::http1);
  //rate_test(number_of_requests, curl::http_version::http2);
  number_of_requests = 1000;
  thread_test(number_of_requests, curl::http_version::http1);
  //thread_test(number_of_requests, curl::http_version::http2);
}
