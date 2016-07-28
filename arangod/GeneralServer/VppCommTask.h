#ifndef ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_VPP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"
#include "lib/Rest/VppResponse.h"
#include "lib/Rest/VppRequest.h"

#include <stdexcept>

namespace arangodb {
namespace rest {

struct VPackMessage {
  VPackMessage() : _buffer(), _header(), _payload() {}
  VPackBuffer<uint8_t> _buffer;
  VPackSlice _header;
  VPackSlice _payload;
};

struct VPackOffsets {
  VPackOffsets(char const* begin, char const* end,
               char const* payloadBegin = nullptr)
      : _begin(begin),
        _end(end),
        _headBegin(begin),
        _headEnd(payloadBegin ? payloadBegin : end),
        _payloadBegin(payloadBegin),
        _payloadEnd(payloadBegin ? end : nullptr) {}
  char const* _begin;
  char const* _end;
  char const* _headBegin;
  char const* _headEnd;
  char const* _payloadBegin;
  char const* _payloadEnd;
};

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
    if (vppResponse == nullptr) {
      throw std::logic_error("invalid response or response Type");
    }
    addResponse(vppResponse, isError);
  };

 protected:
  void completedWriteBuffer() override final;

 private:
  void processRequest();
  // resets the internal state this method can be called to clean up when the
  // request handling aborts prematurely
  void resetState(bool close);

  void addResponse(VppResponse*, bool isError);
  VppRequest* requestAsVpp();

 private:
  using MessageID = uint64_t;

  struct IncompleteVPackMessage {
    uint32_t _length;  // lenght of total message in bytes
    std::size_t _numberOfChunks;
    VPackBuffer<uint8_t> _chunks;
    // std::vector<std::pair<std::size_t, std::size_t>> _chunkOffesesAndLengths;
    // std::vector<std::size_t> _vpackOffsets;  // offset to slice in buffer
  };
  std::unordered_map<MessageID, IncompleteVPackMessage> _incompleteMessages;

  struct ProcessReadVariables {
    uint32_t
        _currentChunkLength;  // size of chunk processed or 0 when expectiong
                              // new chunk
  };
  ProcessReadVariables _processReadVariables;

  struct ChunkHeader {
    uint32_t _length;
    uint32_t _chunk;
    uint64_t _messageId;
    bool _isFirst;
  };

  bool isChunkComplete();         // subfunction of processRead
  ChunkHeader readChunkHeader();  // subfuncaion of processRead

  // user
  // authenticated or not
  // database aus url
};
}  // rest
}  // arangodb

#endif
