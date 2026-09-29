# Server-only image (arangodb/core-*): arangod, the starter and rclone.
# Client tools (arangosh, arangodump, ...) are not part of this image;
# they live in the client-tools image (deploy-alpine-client.dockerfile).
# The CI job that builds this image prunes them from the install tree.
FROM alpine:3.24
LABEL org.opencontainers.image.authors="hackers@arangodb.com"

ARG arch

RUN apk update --no-cache && apk upgrade --no-cache
RUN apk add --no-cache pwgen numactl numactl-tools elfutils

COPY ./install/ /
COPY setup.sh /setup.sh
RUN /setup.sh && rm /setup.sh

# Adjust TZ by default since tzdata package isn't present (BTS-913)
RUN echo "UTC" > /etc/timezone

# The following is magic for unholy OpenShift security business.
# Containers in OpenShift by default run with a random UID but with GID 0,
# and we want that they can access the database and doc directories even
# without a volume mount:
RUN chgrp 0 /var/lib/arangodb4 && \
    chmod 775 /var/lib/arangodb4

COPY entrypoint-alpine.sh /entrypoint.sh
RUN ["chmod", "+x", "/entrypoint.sh"]
ENTRYPOINT [ "/entrypoint.sh" ]

EXPOSE 8529
CMD [ "arangod" ]
