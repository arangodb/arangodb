# Server-only image (arangodb/core-*): arangod, the starter and rclone.
# Client tools (arangosh, arangodump, ...) are not part of this image;
# they live in the client-tools image (deploy-ubuntu-client.dockerfile).
# The CI job that builds this image prunes them from the install tree.

# Required: image providing opt/lib/ossl-modules/fips.so. Docker expands
# every stage's FROM, so it must be set even with USE_FIPS=0; it is only
# pulled with USE_FIPS=1.
ARG ARANGO_BUILD_IMAGE
# 1: ship the OpenSSL FIPS provider and its config (arangodb/coredb-fips).
ARG USE_FIPS=0

# USE_FIPS=0: empty payload
FROM scratch AS fips-0
WORKDIR /fips-files

# USE_FIPS=1: fips provider + config
FROM ${ARANGO_BUILD_IMAGE:?ARANGO_BUILD_IMAGE_must_be_set} AS fips-1-build
FROM scratch AS fips-1
COPY --from=fips-1-build /opt/lib/ossl-modules/fips.so /fips-files/lib/ossl-modules/fips.so
COPY fips-only.cnf /fips-files/ssl/fips-only.cnf

# COPY --from does not expand variables; FROM does (global ARGs only).
FROM fips-${USE_FIPS} AS fips

FROM ubuntu:26.04
LABEL org.opencontainers.image.authors="hackers@arangodb.com"

ARG arch
ARG USE_FIPS

ENV DEBIAN_FRONTEND=noninteractive
ENV USE_FIPS=${USE_FIPS}

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
        pwgen \
        numactl \
        elfutils \
        ca-certificates \
        openssl \
        wget && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY ./install/ /
COPY setup.sh /setup.sh
RUN /setup.sh && rm /setup.sh

# Adjust TZ by default since tzdata package isn't present (BTS-913)
RUN echo "UTC" > /etc/timezone

# FIPS files (only populated when USE_FIPS=1)
COPY --from=fips /fips-files/ /opt/

# The following is magic for unholy OpenShift security business.
# Containers in OpenShift by default run with a random UID but with GID 0,
# and we want that they can access the database and doc directories even
# without a volume mount. /opt/ssl only exists with USE_FIPS=1, where the
# entrypoint writes fipsmodule.cnf into it:
RUN chgrp 0 /var/lib/arangodb4 && \
    chmod 775 /var/lib/arangodb4 && \
    if [ -d /opt/ssl ]; then chgrp 0 /opt/ssl && chmod 775 /opt/ssl; fi

COPY entrypoint-ubuntu.sh /entrypoint.sh
RUN ["chmod", "+x", "/entrypoint.sh"]
ENTRYPOINT [ "/entrypoint.sh" ]

EXPOSE 8529
CMD [ "arangod" ]