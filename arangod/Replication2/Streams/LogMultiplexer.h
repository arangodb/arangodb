#pragma once
#include <utility>

#include <Futures/Future.h>

#include <Replication2/ReplicatedLog/ILogParticipant.h>
#include <Replication2/ReplicatedLog/LogCommon.h>
#include <Replication2/ReplicatedLog/types.h>

#include <Replication2/Streams/StreamSpecification.h>
#include <Replication2/Streams/Streams.h>

namespace arangodb::replication2::replicated_log {
class LogFollower;
class LogLeader;
}  // namespace arangodb::replication2::replicated_log

namespace arangodb::replication2::streams {

/**
 * Common stream dispatcher class for Multiplexer and Demultiplexer. You can
 * obtain a stream given its id using getStreamById. Alternatively, you can
 * static_cast the a pointer to StreamBase<Descriptor> for the given stream.
 * @tparam Self
 * @tparam Spec
 * @tparam StreamType
 */
template <typename Self, typename Spec, template <typename> typename StreamType>
struct LogMultiplexerStreamDispatcher : std::enable_shared_from_this<Self>,
                                        StreamDispatcherBase<Spec, StreamType> {
  template <StreamId Id, typename Descriptor = stream_descriptor_by_id_t<Id, Spec>>
  auto getStreamBaseById()
      -> std::shared_ptr<StreamGenericBase<Descriptor, StreamType>> {
    return getStreamByDescriptor<Descriptor>();
  }

  template <StreamId Id>
  auto getStreamById() -> std::shared_ptr<StreamType<stream_type_by_id_t<Id, Spec>>> {
    return getStreamByDescriptor<stream_descriptor_by_id_t<Id, Spec>>();
  }

  template <typename Descriptor>
  auto getStreamByDescriptor()
      -> std::shared_ptr<StreamGenericBase<Descriptor, StreamType>> {
    return std::static_pointer_cast<StreamGenericBase<Descriptor, StreamType>>(
        this->shared_from_this());
  }
};

/**
 * Demultiplexer class. Use ::construct to create an instance.
 * @tparam Spec Log specification
 */
template <typename Spec>
struct LogDemultiplexer
    : LogMultiplexerStreamDispatcher<LogDemultiplexer<Spec>, Spec, Stream> {
  virtual auto digestIterator(LogRangeIterator& iter) -> void = 0;
  virtual auto listen() -> void = 0;

  static auto construct(std::shared_ptr<arangodb::replication2::replicated_log::ILogParticipant>)
      -> std::shared_ptr<LogDemultiplexer>;

 protected:
  LogDemultiplexer() = default;
};

/**
 * Multiplexer class. Use ::construct to create an instance.
 * @tparam Spec Log specification
 */
template <typename Spec>
struct LogMultiplexer
    : LogMultiplexerStreamDispatcher<LogMultiplexer<Spec>, Spec, ProducerStream> {
  static auto construct(std::shared_ptr<arangodb::replication2::replicated_log::LogLeader> leader)
      -> std::shared_ptr<LogMultiplexer>;

 protected:
  LogMultiplexer() = default;
};

}  // namespace arangodb::replication2::streams
