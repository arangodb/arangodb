#include "curl-stuff.h"
#include <iostream>
#include <latch>

#include "curl/curl.h"

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

void send_request(curl::connection_pool& pool, std::latch& done, int counter) {
  if (counter == 0) {
    done.count_down();
  } else {
    curl::request_options options;
    curl::send_request(
        pool, arangodb::network::curl::http_method::kGet,
        "http://localhost:8529", "http://localhost:8529/_api/version", {},
        options, [&, counter](curl::response const& response, int code) {
          if (code != 0) {
            std::cout << "CODE = " << curl_easy_strerror(CURLcode(code))
                      << std::endl;
          }
          send_request(pool, done, counter - 1);
        });
  }
}

int main(int argc, char* argv[]) {
  curl::connection_pool pool;

  constexpr auto number_of_requests = 10000;
  {
    std::latch latch(number_of_requests);
    auto const start = std::chrono::steady_clock::now();

    rate(std::chrono::microseconds{50}, number_of_requests, [&] {
      curl::request_options options;
      curl::send_request(
          pool, arangodb::network::curl::http_method::kGet,
          "http://localhost:8529", "http://localhost:8529/_api/version", {},
          options, [&](curl::response const& response, int code) {
            if (code != 0) {
              std::cout << "CODE = " << curl_easy_strerror(CURLcode(code))
                        << std::endl;
            }
            latch.count_down();
          });
    });

    std::cout << "done sending\n";
    latch.wait();
    auto const end = std::chrono::steady_clock::now();

    auto const seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    std::cout << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  }

  {
    std::latch latch(1);
    auto const start = std::chrono::steady_clock::now();
    send_request(pool, latch, number_of_requests);
    latch.wait();
    auto const end = std::chrono::steady_clock::now();

    auto const seconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    std::cout << number_of_requests << " took " << seconds
              << " rps = " << ((double)number_of_requests / seconds.count())
              << std::endl;
  }
}
