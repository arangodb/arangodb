////////////////////////////////////////////////////////////////////////////////
/// @brief stores a message in a ring buffer
///
/// We ignore any race conditions here.
////////////////////////////////////////////////////////////////////////////////

static void StoreMessage(LogLevel level, std::string const& message,
                         size_t offset) {
  MUTEX_LOCKER(guard, RingBufferLock);

  uint64_t n = RingBufferId++;
  LogBuffer* ptr = &RingBuffer[n % RING_BUFFER_SIZE];

  ptr->_id = n;
  ptr->_level = level;
  ptr->_timestamp = time(0);
  TRI_CopyString(ptr->_message, message.c_str() + offset,
                 sizeof(ptr->_message) - 1);
}


static Mutex Logger::_ringBufferLock;
static uint64_t Logger::_ringBufferId = 0;
static LogBuffer Logger::_ringBuffer[RING_BUFFER_SIZE];



////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last log entries
////////////////////////////////////////////////////////////////////////////////

std::vector<LogBuffer> Logger::bufferedEntries(LogLevel level, uint64_t start,
                                               bool upToLevel) {
  std::vector<LogBuffer> result;

  MUTEX_LOCKER(guard, RingBufferLock);

  size_t s = 0;
  size_t e;

  if (RingBufferId >= RING_BUFFER_SIZE) {
    s = e = (RingBufferId + 1) % RING_BUFFER_SIZE;
  } else {
    e = RingBufferId;
  }

  for (size_t i = s; i != e;) {
    LogBuffer& p = RingBuffer[i];

    if (p._id >= start) {
      if (upToLevel) {
        if ((int)p._level <= (int)level) {
          result.emplace_back(p);
        }
      } else {
        if (p._level == level) {
          result.emplace_back(p);
        }
      }
    }

    ++i;

    if (i >= RING_BUFFER_SIZE) {
      i = 0;
    }
  }

  return result;
}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the last log entries
  //////////////////////////////////////////////////////////////////////////////

  static std::vector<LogBuffer> bufferedEntries(LogLevel level, uint64_t start,
                                                bool upToLevel);

#include <iostream>

