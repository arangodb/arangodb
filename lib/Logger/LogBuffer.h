////////////////////////////////////////////////////////////////////////////////
/// @brief LogBuffer
///
/// This class is used to store a number of log messages in the server
/// for retrieval. This messages are truncated and overwritten without
/// warning.
////////////////////////////////////////////////////////////////////////////////

struct LogBuffer {
  static size_t const RING_BUFFER_SIZE = 10240;

  uint64_t _id;
  LogLevel _level;
  time_t _timestamp;
  char _message[256];
};

  static Mutex _ringBufferLock;
  static uint64_t _ringBufferId;
  static LogBuffer _ringBuffer[];

  static void StoreMessage(LogLevel level, std::string const& message, size_t offset);
