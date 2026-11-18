# Client-tools-only image (arangodb/client-tools-*): the same
# install tree as the server image, pruned of the server bits (arangod,
# the starter, rclone, server config/data/service files) by the CI job
# that builds this image — mirroring the TAR.GZ client bundle's contents
# (build-targz.sh).
FROM ubuntu:26.04
LABEL org.opencontainers.image.authors="hackers@arangodb.com"

ARG arch
ARG DEBIAN_FRONTEND=noninteractive

# ca-certificates: ubuntu:26.04 ships no CA bundle.
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY ./install-client/ /

# Client tools need no privileges: run as a dedicated non-root user.
# The home directory is created so arangosh can persist
# .arangosh.history there.
RUN groupadd --system arangodb && \
    useradd --system --gid arangodb --create-home arangodb
USER arangodb

CMD [ "arangosh" ]
