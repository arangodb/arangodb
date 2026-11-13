FROM docker.io/confluentinc/cp-schema-registry:7.5.2

COPY --chmod=755 wait-for-kafka.sh /usr/local/bin/wait-for-kafka.sh

ENTRYPOINT ["/usr/local/bin/wait-for-kafka.sh"]
CMD ["/etc/confluent/docker/run"]
