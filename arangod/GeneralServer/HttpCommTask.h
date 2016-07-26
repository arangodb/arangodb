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

  bool processRead() override;  // called by handleRead
  virtual void processRequest() override;

  void addResponse(GeneralResponse* response) override {
    // convert from GeneralResponse to httpResponse ad dispatch request to class
    // internal addResponse
    HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
    if (httpResponse == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    addResponse(httpResponse);
  };

 protected:
  void completedWriteBuffer() override final;

 private:
  bool handleRead() override final;  // required by SocketTask

  void signalTask(TaskData*) override final;
  // resets the internal state this method can be called to clean up when the
  // request handling aborts prematurely
  virtual void resetState(bool close) override final;

  HttpRequest* requestAsHttp();
  void addResponse(HttpResponse*);
  void finishedChunked();
  // check the content-length header of a request and fail it is broken
  bool checkContentLength(bool expectContentLength);
  void processCorsOptions();                // handles CORS options
  std::string authenticationRealm() const;  // returns the authentication realm
  GeneralResponse::ResponseCode
  authenticateRequest();                  // checks the authentication
  void sendChunk(basics::StringBuffer*);  // sends more chunked data

 private:
  size_t _readPosition;   // current read position
  size_t _startPosition;  // start position of current request
  size_t _bodyPosition;   // start of the body position
  size_t _bodyLength;     // body length
  bool _closeRequested;   // true if a close has been requested by the client
  bool _readRequestBody;  // true if reading the request body
  bool _allowMethodOverride;  // allow method override
  bool _denyCredentials;  // whether or not to allow credentialed requests (only
                          // CORS)
  bool _acceptDeflate;    // whether the client accepts deflate algorithm
  bool _newRequest;       // new request started
  GeneralRequest::RequestType _requestType;  // type of request (GET, POST, ...)
  std::string _fullUrl;                      // value of requested URL
  std::string _origin;  // value of the HTTP origin header the client sent (if
                        // any, CORS only)
  size_t
      _sinceCompactification;  // number of requests since last compactification
  size_t _originalBodyLength;

  // authentication real
  std::string const _authenticationRealm;
};
}  // rest
}  // arangodb

#endif
