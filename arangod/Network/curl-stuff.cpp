#include "curl-stuff.h"

#include "Logger/LogMacros.h"
#include "Basics/application-exit.h"
#include "Basics/StringUtils.h"

using namespace arangodb::network::curl;
using namespace arangodb::network;
using namespace arangodb;

namespace {
std::atomic<uint64_t> nextRequestId = 0;
}

struct curl::request {
  std::string url;
  std::string body;
  std::size_t read_offset;
  curl_easy_handle _curl_handle;
  curl_slist* _curl_headers;
  std::function<void(response, CURLcode)> _callback;

  std::uint64_t unique_id;

  response _response;
  ~request();
};

namespace {

static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                             void* userdata) {
  size_t total_size = size * nmemb;
  auto req = static_cast<request*>(userdata);
  req->_response.body.append(std::string_view{ptr, total_size});
  return total_size;
}

int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t ultotal, curl_off_t ulnow) {
  // auto req = static_cast<request*>(clientp);
  //  std::cout << "DL Total: " << dltotal << " DL now: " << dlnow
  //            << " Up Total: " << ultotal << " Up now: " << ulnow <<
  //            std::endl;
  return 0;
}

size_t header_callback(char* buffer, size_t size, size_t nitems,
                       void* userdata) {
  auto req = static_cast<request*>(userdata);
  auto line = std::string_view{buffer, size * nitems};

  if (line.starts_with("HTTP/2")) {
    req->_response.headers.clear();
    return line.size();
  }

  auto separator = line.find(':');
  if (separator == std::string_view::npos) {
    return line.size();
  }

  if (!line.ends_with("\r\n")) {
    return line.size();
  }

  auto header = line.substr(0, separator);
  auto value = line.substr(separator + 1);
  while (value.starts_with(' ')) {
    value.remove_prefix(1);
  }
  value.remove_suffix(2);

  req->_response.headers.emplace(basics::StringUtils::tolower(header), value);
  return line.size();
}

[[maybe_unused]] size_t read_callback(char* buffer, size_t size, size_t nitems,
                                      void* userdata) {
  auto req = static_cast<request*>(userdata);
  size_t total_size = size * nitems;
  size_t bytes_available = req->body.size() - req->read_offset;
  size_t bytes_written = std::min(total_size, bytes_available);

  std::memcpy(buffer, req->body.c_str() + req->read_offset, bytes_written);

  req->read_offset += bytes_written;
  return bytes_written;
}

#define LOG_DEVEL_CURL LOG_DEVEL_IF(false)

int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size,
                   void* clientp) {
  auto req = static_cast<request*>(clientp);
  std::string_view prefix = "CURL: ";
  switch (type) {
    case CURLINFO_HEADER_IN:
      prefix = "HDR-IN: ";
      break;
    case CURLINFO_HEADER_OUT:
      prefix = "HRD-OUT: ";
      break;
    case CURLINFO_SSL_DATA_IN:
      prefix = "SSL-IN: ";
      break;
    case CURLINFO_SSL_DATA_OUT:
      prefix = "SSL-OUT: ";
      break;
    default:
      break;
  }

  LOG_DEVEL_CURL << "[" << req->unique_id << "] " << prefix
                 << std::string_view{data, size};
  return 0;
}

}  // namespace

void arangodb::network::curl::send_request(
    connection_pool& pool, http_method method, std::string path,
    std::string body, request_options const& options,
    std::function<void(response, CURLcode)> callback) {
  auto req = std::make_unique<request>();

  req->url = std::move(path);
  req->body = std::move(body);
  req->_callback = std::move(callback);
  req->unique_id = nextRequestId.fetch_add(1, std::memory_order_relaxed);

  LOG_DEVEL_CURL << "[" << req->unique_id << "] URL " << req->url;

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_URL,
                   req->url.c_str());

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_WRITEFUNCTION,
                   &write_callback);
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_WRITEDATA,
                   req.get());

  // curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_READFUNCTION,
  //                  &read_callback);
  // curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_READDATA,
  // req.get());

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_XFERINFOFUNCTION,
                   &progress_callback);
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_XFERINFODATA,
                   req.get());

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_HEADERFUNCTION,
                   &header_callback);
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_HEADERDATA,
                   req.get());

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_PRIVATE, req.get());
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_DEBUGDATA,
                   req.get());

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_TIMEOUT_MS,
                   options.timeout.count());

  curl_slist* headers = nullptr;
  std::string line;
  for (auto const& [key, value] : options.header) {
    line.clear();
    line.reserve(key.size() + value.size() + 2);
    line += key;
    if (value.empty()) {
      line += ';';  // special empty header handling
    } else {
      line += ": ";
      line += value;
    }
    headers = curl_slist_append(headers, line.c_str());
    LOG_DEVEL_CURL << "[" << req->unique_id << "] HDR " << line;
  }

  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POSTFIELDSIZE_LARGE,
                   (curl_off_t)req->body.size());
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POSTFIELDS,
                   req->body.data());

  switch (method) {
    case http_method::kGet:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_HTTPGET, 1l);
      break;
    case http_method::kPost:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POST, 1l);
      break;
    case http_method::kPut:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POST, 1l);
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_CUSTOMREQUEST,
                       "PUT");
      break;
    case http_method::kDelete:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POST, 1l);
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_CUSTOMREQUEST,
                       "DELETE");
      break;
    case http_method::kPatch:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_POST, 1l);
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_CUSTOMREQUEST,
                       "PATCH");
      break;
    case http_method::kHead:
      curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_NOBODY, 1l);
      break;
  }

  req->_curl_headers = headers;
  curl_easy_setopt(req->_curl_handle._easy_handle, CURLOPT_HTTPHEADER, headers);

  pool.push(std::move(req));
}

size_t connection_pool::drain_msg_queue() noexcept {
  size_t num_message = 0;
  int msgs_in_queue = 0;
  while (CURLMsg* msg =
             curl_multi_info_read(_curl_multi._multi_handle, &msgs_in_queue)) {
    if (msg->msg == CURLMSG_DONE) {
      resolve_handle(msg->easy_handle, msg->data.result);
      num_message += 1;
    }
  }

  return num_message;
}

void connection_pool::resolve_handle(CURL* easy_handle,
                                     CURLcode result) noexcept {
  void* req_ptr;
  curl_easy_getinfo(easy_handle, CURLINFO_PRIVATE, &req_ptr);
  auto req = std::unique_ptr<request>(static_cast<request*>(req_ptr));

  curl_multi_remove_handle(_curl_multi._multi_handle, easy_handle);

  curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &req->_response.code);
  try {
    req->_callback(std::move(req->_response), result);
  } catch (std::exception const& e) {
    LOG_TOPIC("d0653", ERR, Logger::COMMUNICATION)
        << "exception caught in callback: " << e.what();
  }
}

void connection_pool::install_new_handles() noexcept {
  std::vector<std::unique_ptr<request>> requests;

  while (true) {
    {
      std::unique_lock guard(_mutex);
      std::swap(requests, _queue);
    }

    if (requests.empty()) {
      break;
    }

    CURLMcode result;
    for (auto& req : requests) {
      result = curl_multi_add_handle(_curl_multi._multi_handle,
                                     req->_curl_handle._easy_handle);
      if (result != CURLM_OK) {
        LOG_TOPIC("f2be4", FATAL, Logger::COMMUNICATION)
            << "curl_multi_add_handle failed: " << curl_multi_strerror(result);
        FATAL_ERROR_ABORT();
      }
      std::ignore = req.release();
    }

    requests.clear();
  }
}
void connection_pool::run_curl_loop(std::stop_token stoken) noexcept {
  int running_handles = 0;
  do {
    install_new_handles();

    running_handles = _curl_multi.perform();
    size_t num_messages = drain_msg_queue();
    if (num_messages > 0) {
      continue;
    }

    if (running_handles == 0) {
      std::unique_lock guard(_mutex);
      _cv.wait(guard, stoken, [this] { return !_queue.empty(); });
    } else {
      _curl_multi.poll();
    }

  } while (!stoken.stop_requested() || running_handles != 0);
}

void connection_pool::push(std::unique_ptr<request>&& req) {
  {
    std::unique_lock guard(_mutex);
    _queue.emplace_back(std::move(req));
  }
  _curl_multi.notify();
  _cv.notify_all();
}

connection_pool::~connection_pool() {
  std::unique_lock guard(_mutex);
  _cv.notify_all();
}

connection_pool::connection_pool()
    : _curl_thread(
          [this](std::stop_token stoken) { this->run_curl_loop(stoken); }) {}

request::~request() {
  if (_curl_headers != NULL) {
    curl_slist_free_all(_curl_headers);
  }
}

curl_easy_handle::curl_easy_handle() : _easy_handle(curl_easy_init()) {
  if (!_easy_handle) {
    throw std::runtime_error("curl_easy_init failed");
  }

  // curl_easy_setopt(_easy_handle, CURLOPT_HTTP_VERSION,
  // CURL_HTTP_VERSION_2_0);
  curl_easy_setopt(_easy_handle, CURLOPT_PIPEWAIT, 1l);
  curl_easy_setopt(_easy_handle, CURLOPT_SSL_ENABLE_ALPN, 1l);
  curl_easy_setopt(_easy_handle, CURLOPT_SSL_VERIFYPEER, 0l);
  curl_easy_setopt(_easy_handle, CURLOPT_SSL_VERIFYHOST, 0l);
  curl_easy_setopt(_easy_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);

  curl_easy_setopt(_easy_handle, CURLOPT_PROTOCOLS_STR, "HTTP,HTTPS");

  curl_easy_setopt(_easy_handle, CURLOPT_TRANSFER_ENCODING, 0l);
  curl_easy_setopt(_easy_handle, CURLOPT_ACCEPT_ENCODING, NULL);
  // we do decoding on our own
  curl_easy_setopt(_easy_handle, CURLOPT_HTTP_CONTENT_DECODING, 0l);

  curl_easy_setopt(_easy_handle, CURLOPT_VERBOSE, 1l);
  curl_easy_setopt(_easy_handle, CURLOPT_DEBUGFUNCTION, &debug_callback);
  curl_easy_setopt(_easy_handle, CURLOPT_NOSIGNAL, 1l);
  curl_easy_setopt(_easy_handle, CURLOPT_NOPROGRESS, 0l);
  curl_easy_setopt(_easy_handle, CURLOPT_WILDCARDMATCH, 0l);
  curl_easy_setopt(_easy_handle, CURLOPT_HTTP09_ALLOWED, 0l);

  curl_easy_setopt(_easy_handle, CURLOPT_UPLOAD_BUFFERSIZE, 12000L);
}

curl_easy_handle::~curl_easy_handle() {
  if (_easy_handle) {
    curl_easy_cleanup(_easy_handle);
  }
}

curl_multi_handle::curl_multi_handle() : _multi_handle(curl_multi_init()) {
  if (!_multi_handle) {
    throw std::runtime_error("curl_multi_init failed");
  }
  curl_multi_setopt(_multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
  curl_multi_setopt(_multi_handle, CURLMOPT_MAX_CONCURRENT_STREAMS, 2000l);
}

curl_multi_handle::~curl_multi_handle() {
  if (_multi_handle) {
    curl_multi_cleanup(_multi_handle);
  }
}

int curl_multi_handle::perform() noexcept {
  int running_handles;
  auto result = curl_multi_perform(_multi_handle, &running_handles);
  if (result != CURLM_OK) {
    LOG_TOPIC("d1f1f", FATAL, Logger::COMMUNICATION)
        << "curl_multi_perform failed: " << curl_multi_strerror(result);
    FATAL_ERROR_ABORT();
  }
  return running_handles;
}

void curl_multi_handle::poll() noexcept {
  auto result = curl_multi_poll(_multi_handle, NULL, 0, 500, NULL);
  if (result != CURLM_OK) {
    std::cerr << "curl_multi_poll failed: " << curl_multi_strerror(result)
              << std::endl;
    abort();
  }
}

void curl_multi_handle::notify() { curl_multi_wakeup(_multi_handle); }
