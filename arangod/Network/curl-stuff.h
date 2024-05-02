#pragma once

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <deque>
#include <condition_variable>
#include <cstring>
#include <sstream>

namespace arangodb::network::curl {

struct curl_multi_handle {
  curl_multi_handle();

  ~curl_multi_handle();

  int perform() noexcept;

  void poll() noexcept;

  void notify();

  CURLM* _multi_handle = nullptr;
};

struct curl_easy_handle {
  curl_easy_handle();

  ~curl_easy_handle();

  CURL* _easy_handle = nullptr;
};

struct request_options {
  std::unordered_map<std::string, std::string> header;
  std::chrono::milliseconds timeout = std::chrono::seconds{120};
};

enum class http_method { kGet, kPost, kPut, kDelete, kHead, kPatch };

struct response {
  long code;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  std::stringstream debug_string;
};

struct request;

struct connection_pool {
  connection_pool();

  void push(std::unique_ptr<request>&& req);

  ~connection_pool();

  void stop();

  void cancelConnections(std::string endpoint);

 private:
  void run_curl_loop(std::stop_token stoken) noexcept;

  void install_new_handles() noexcept;

  void resolve_handle(CURL* easy_handle, CURLcode result) noexcept;

  size_t drain_msg_queue() noexcept;

  curl_multi_handle _curl_multi;
  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::vector<std::unique_ptr<request>> _queue;
  std::unordered_set<std::string> _cancelEndpoints;
  std::mutex _requestsPerEndpointMutex;
  std::unordered_map<std::string, std::unordered_set<CURL*>>
      _requestsPerEndpoint;
  std::jthread _curl_thread;
};

// TODO endpoint is redundant (part of path)
void send_request(connection_pool& pool, http_method method,
                  std::string endpoint, std::string path, std::string body,
                  request_options const& options,
                  std::function<void(response, CURLcode)> callback);
}  // namespace arangodb::network::curl
