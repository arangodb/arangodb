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

    std::this_thread::sleep_for(d);
  }
}

int main(int argc, char* argv[]) {
  curl::connection_pool pool;

  const auto number_of_requests = 1000;
  std::latch latch(number_of_requests);

  rate(std::chrono::milliseconds{10}, number_of_requests, [&] {
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

  latch.wait();
}
