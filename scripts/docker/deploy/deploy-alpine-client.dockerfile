# Client-tools-only image (arangodb/client-tools-*): the same
# install tree as the server image, pruned of the server bits (arangod,
# the starter, rclone, server config/data/service files) by the CI job
# that builds this image — mirroring the TAR.GZ client bundle's contents
# (build-targz.sh).
FROM alpine:3.24
LABEL org.opencontainers.image.authors="hackers@arangodb.com"

ARG arch

RUN apk update --no-cache && apk upgrade --no-cache

COPY ./install-client/ /

# Client tools need no privileges: run as a dedicated non-root user.
RUN addgroup -S arangodb && adduser -S -G arangodb -H -D arangodb
USER arangodb

CMD [ "arangosh" ]
