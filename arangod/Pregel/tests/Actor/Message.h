struct Message {
  Message(ActorPID sender, ActorPID receiver,
          std::unique_ptr<MessagePayload> payload)
      : sender(sender), receiver(receiver), payload(std::move(payload)) {}

  ActorPID sender;
  ActorPID receiver;

  std::unique_ptr<MessagePayload> payload;
};
