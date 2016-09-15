#ifndef ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"
namespace arangodb {
namespace rest {

class HttpCommTask : public GeneralCommTask {
 public:
  static size_t const MaximalHeaderSize;
  static size_t const MaximalBodySize;
  static size_t const MaximalPipelineSize;
  static size_t const RunCompactEvery;

 public:
  HttpCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&, double timeout);

  // convert from GeneralResponse to httpResponse ad dispatch request to class
  // internal addResponse
  void addResponse(GeneralResponse* response) override {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
    if (httpResponse == nullptr) {
      throw std::logic_error("invalid response or response Type");
    }
    addResponse(httpResponse);
  };

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::HTTP;
  };

 protected:
  bool processRead() override;

  void handleChunk(char const*, size_t) override final;

  std::unique_ptr<GeneralResponse> createResponse(
      rest::ResponseCode, uint64_t messageId) override final;

  void handleSimpleError(rest::ResponseCode code,
                         uint64_t messageId = 1) override final;

  void handleSimpleError(rest::ResponseCode, int code,
                         std::string const& errorMessage,
                         uint64_t messageId = 1) override final;

 private:
  void processRequest(std::unique_ptr<HttpRequest>);
  void processCorsOptions(std::unique_ptr<HttpRequest>);

  void resetState();

  void addResponse(HttpResponse*);

  void finishedChunked();
  // check the content-length header of a request and fail it is broken
  bool checkContentLength(HttpRequest*, bool expectContentLength);
  std::string authenticationRealm() const;  // returns the authentication realm
  rest::ResponseCode authenticateRequest(HttpRequest*);

 private:
  size_t _readPosition;       // current read position
  size_t _startPosition;      // start position of current request
  size_t _bodyPosition;       // start of the body position
  size_t _bodyLength;         // body length
  bool _readRequestBody;      // true if reading the request body
  bool _allowMethodOverride;  // allow method override
  bool _denyCredentials;  // whether or not to allow credentialed requests (only
                          // CORS)
  bool _newRequest;       // new request started
  // TODO(fc) remove
  rest::RequestType _requestType;  // type of request (GET, POST, ...)
  std::string _fullUrl;                      // value of requested URL
  std::string _origin;  // value of the HTTP origin header the client sent (if
                        // any, CORS only)
  size_t
      _sinceCompactification;  // number of requests since last compactification
  size_t _originalBodyLength;

  // authentication real
  std::string const _authenticationRealm;

  // true if within a chunked response
  bool _isChunked = false;

  // true if request is complete but not handled
  bool _requestPending = false;

  std::unique_ptr<HttpRequest> _incompleteRequest;
};
}
}

#endif
