#ifndef ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"
#include "lib/Rest/VppResponse.h"
#include "lib/Rest/VppRequest.h"
namespace arangodb {
namespace rest {

class VppCommTask : public GeneralCommTask {
 public:
  VppCommTask(GeneralServer*, TRI_socket_t, ConnectionInfo&&, double timeout);

  // read data check if chunk and message are complete
  // if message is complete exectue a request
  bool processRead() override;

  // convert from GeneralResponse to vppResponse ad dispatch request to class
  // internal addResponse
  void addResponse(GeneralResponse* response, bool isError) override {
    VppResponse* vppResponse = dynamic_cast<VppResponse*>(response);
    if (vppResponse != nullptr) {
      addResponse(vppResponse, isError);
    }
    // else throw? do nothing?!!??!!
  };

 protected:
  void handleChunk(char const*, size_t) override final;
  void completedWriteBuffer() override final;

 private:
  void processRequest();
  // resets the internal state this method can be called to clean up when the
  // request handling aborts prematurely
  void resetState(bool close);

  void addResponse(VppResponse*, bool isError);

  VppRequest* requestAsVpp();
  void finishedChunked();
  // check the content-length header of a request and fail it is broken
  bool checkContentLength(bool expectContentLength);

  std::string authenticationRealm() const;  // returns the authentication realm
  GeneralResponse::ResponseCode
  authenticateRequest();                  // checks the authentication
  void sendChunk(basics::StringBuffer*);  // sends more chunked data

 private:
  size_t _readPosition;       // current read position
  size_t _startPosition;      // start position of current request
  size_t _bodyPosition;       // start of the body position
  size_t _bodyLength;         // body length
  bool _readRequestBody;      // true if reading the request body
  bool _allowMethodOverride;  // allow method override
  bool _denyCredentials;  // whether or not to allow credentialed requests (only
                          // CORS)
  bool _acceptDeflate;    // whether the client accepts deflate algorithm
  bool _newRequest;       // new request started
  GeneralRequest::RequestType _requestType;  // type of request (GET, POST, ...)
  std::string _fullUrl;                      // value of requested URL
  std::string _origin;  // value of the vpp origin header the client sent (if
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
};
}  // rest
}  // arangodb

#endif
