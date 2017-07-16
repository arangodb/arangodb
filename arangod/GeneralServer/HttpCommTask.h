#ifndef ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H 1

#include "Basics/Common.h"
#include "GeneralServer/GeneralCommTask.h"
#include "Rest/HttpResponse.h"

namespace arangodb {
class HttpRequest;

namespace rest {
class HttpCommTask final : public GeneralCommTask {
 public:
  static size_t const MaximalHeaderSize;
  static size_t const MaximalBodySize;
  static size_t const MaximalPipelineSize;
  static size_t const RunCompactEvery;

 public:
  HttpCommTask(EventLoop, GeneralServer*, std::unique_ptr<Socket> socket,
               ConnectionInfo&&, double timeout);

  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::HTTP;
  }

  // convert from GeneralResponse to httpResponse
  void addResponse(GeneralResponse* response,
                   RequestStatistics* stat) override {
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);

    if (httpResponse == nullptr) {
      throw std::logic_error("invalid response or response Type");
    }

    addResponse(httpResponse, stat);
  }

 private:
  bool processRead(double startTime) override;
  void compactify() override;

  std::unique_ptr<GeneralResponse> createResponse(
      rest::ResponseCode, uint64_t messageId) override final;

  void handleSimpleError(rest::ResponseCode code, GeneralRequest const&,
                         uint64_t messageId = 1) override final;

  void handleSimpleError(rest::ResponseCode, GeneralRequest const&, int code,
                         std::string const& errorMessage,
                         uint64_t messageId = 1) override final;

  bool allowDirectHandling() const override final { return true; }

 private:
  void processRequest(std::unique_ptr<HttpRequest>);
  void processCorsOptions(std::unique_ptr<HttpRequest>);

  void resetState();

  void addResponse(HttpResponse*, RequestStatistics* stat);

  // check the content-length header of a request and fail it is broken
  bool checkContentLength(HttpRequest*, bool expectContentLength);

  std::string authenticationRealm() const;
  ResponseCode authenticateRequest(HttpRequest*);
  ResponseCode handleAuthHeader(HttpRequest* request) const;
  
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
  rest::RequestType _requestType;  // type of request (GET, POST, ...)
  std::string _fullUrl;            // value of requested URL
  std::string _origin;  // value of the HTTP origin header the client sent (if
                        // any, CORS only)
  size_t
      _sinceCompactification;  // number of requests since last compactification
  size_t _originalBodyLength;

  std::string const _authenticationRealm;

  // true if request is complete but not handled
  bool _requestPending = false;

  std::unique_ptr<HttpRequest> _incompleteRequest;
};
}
}

#endif
