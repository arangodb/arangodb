#pragma once

#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <deque>
#include <condition_variable>
#include <cstring>
#include <sstream>

#include "fuerte/types.h"

struct Curl_easy;
struct Curl_multi;

namespace arangodb::network::curl {

struct curl_multi_handle {
  curl_multi_handle();

  ~curl_multi_handle();

  int perform() noexcept;

  void poll() noexcept;

  void notify();

  Curl_multi* _multi_handle = nullptr;
};

struct curl_easy_handle {
  curl_easy_handle();
  curl_easy_handle(curl_easy_handle&&);
  ~curl_easy_handle();

  void reset_handle();

  operator bool() { return _easy_handle != nullptr; }

  Curl_easy* _easy_handle = nullptr;
};

struct curl_easy_handle_pool {
  explicit curl_easy_handle_pool(size_t max_handles);

  curl_easy_handle acquire();

  void release(curl_easy_handle&& handle);

 private:
  // TODO this can be implemented in a lock-free fashion using atomics and
  //  linked lists
  std::mutex _mutex;
  std::vector<curl_easy_handle> _handles;
};

struct curl_easy_handle_guard {
  explicit curl_easy_handle_guard(curl_easy_handle_pool* pool);
  explicit curl_easy_handle_guard(curl_easy_handle_guard&&) = default;

  ~curl_easy_handle_guard();

  curl_easy_handle& operator*() { return handle; }
  curl_easy_handle* operator->() { return &handle; }

  curl_easy_handle const& operator*() const { return handle; }
  curl_easy_handle const* operator->() const { return &handle; }

  curl_easy_handle_pool* const pool;
  curl_easy_handle handle;
};

struct request_options {
  std::unordered_map<std::string, std::string> header;
  std::chrono::milliseconds timeout = std::chrono::seconds{120};
};

enum class http_method { kGet, kPost, kPut, kDelete, kHead, kPatch };

struct response {
  std::uint64_t unique_id;
  long code;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
  std::stringstream debug_string;
};

struct request;

enum class http_version { http1 = 0, http2 };

struct connection_pool {
  connection_pool(http_version = {});

  void push(std::unique_ptr<request>&& req);

  ~connection_pool();

  void stop();

  void cancelConnections(std::string endpoint);

  curl_easy_handle_pool handlePool;

  http_version const httpVersion;

 private:
  void run_curl_loop(std::stop_token stoken) noexcept;

  void install_new_handles() noexcept;

  void resolve_handle(Curl_easy* easy_handle, int result) noexcept;

  size_t drain_msg_queue() noexcept;

  curl_multi_handle _curl_multi;
  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::vector<std::unique_ptr<request>> _queue;
  std::unordered_set<std::string> _cancelEndpoints;
  std::mutex _requestsPerEndpointMutex;
  std::unordered_map<std::string, std::unordered_set<Curl_easy*>>
      _requestsPerEndpoint;
  std::jthread _curlThread;
};

struct multi_connection_pool {
  explicit multi_connection_pool(size_t num, curl::http_version httpVersion);

  curl::connection_pool& next_pool();
 private:
  std::atomic<uint64_t> counter;
  std::vector<std::unique_ptr<curl::connection_pool>> pools;
};

// TODO endpoint is redundant (part of path)
void send_request(connection_pool& pool, http_method method,
                  std::string endpoint, std::string path, std::string body,
                  request_options const& options,
                  std::function<void(response, int)> callback);

arangodb::fuerte::Error curlErrorToFuerte(int err);
const char* curlStrError(int err);
}  // namespace arangodb::network::curl
