#pragma once

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <functional>
#include <deque>
#include <condition_variable>
#include <cstring>

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
};

enum class http_method { kGet, kPost, kPut, kDelete, kHead, kPatch };

struct response {
  long code;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct request {
  std::string url;
  std::string body;
  std::size_t read_offset;
  curl_easy_handle _curl_handle;
  curl_slist* _curl_headers;
  std::function<void(response, CURLcode)> _callback;

  response _response;

  ~request();
};

struct connection_pool {
  connection_pool();

  void push(std::unique_ptr<request>&& req);

  ~connection_pool();

 private:
  void run_curl_loop(std::stop_token stoken) noexcept;

  void install_new_handles() noexcept;

  void resolve_handle(CURL* easy_handle, CURLcode result) noexcept;

  size_t drain_msg_queue() noexcept;

  curl_multi_handle _curl_multi;
  std::mutex _mutex;
  std::condition_variable_any _cv;
  std::vector<std::unique_ptr<request>> _queue;
  std::jthread _curl_thread;
};

void send_request(connection_pool& pool, http_method method, std::string path,
                  std::string body, request_options const& options,
                  std::function<void(response, CURLcode)> callback);
}  // namespace arangodb::network::curl
