FROM registry.access.redhat.com/ubi9/ubi-minimal:latest
MAINTAINER Frank Celler <info@arangodb.com>

ARG arch

RUN microdnf update -y && rm -rf /var/cache/yum

# Metadata (required)
ARG name
ARG vendor
ARG version
ARG release
ARG summary
ARG description
ARG maintainer

LABEL name="$name" \
      vendor="$vendor" \
      version="$version" \
      release="$release" \
      summary="$summary" \
      description="$description" \
      maintainer="$maintainer"

ADD ./LICENSE /licenses/LICENSE

RUN PACKAGES_REPO="https://mirror.stream.centos.org/9-stream/BaseOS/$(uname -p)/os/Packages" && \
    NUMA_PACKAGES_VERSION="2.0.16-3.el9.$(uname -p)" && \
    microdnf -y install gpg wget binutils elfutils && \
    for i in numactl-libs numactl; do \
      curl -L -O $PACKAGES_REPO/$i-$NUMA_PACKAGES_VERSION.rpm && \
      rpm -i $i-$NUMA_PACKAGES_VERSION.rpm && \
      rm -f $i-$NUMA_PACKAGES_VERSION.rpm; \
    done && \
    microdnf clean all

COPY ./install/ /
COPY setup-ubi.sh /setup.sh
RUN /setup.sh && rm /setup.sh

# Adjust TZ by default since tzdata package isn't present (BTS-913)
RUN echo "UTC" > /etc/timezone

# The following is magic for unholy OpenShift security business.
# Containers in OpenShift by default run with a random UID but with GID 0,
# and we want that they can access the database and doc directories even
# without a volume mount:
RUN chgrp 0 /var/lib/arangodb3 /var/lib/arangodb3-apps && \
    chmod 775 /var/lib/arangodb3 /var/lib/arangodb3-apps

# retain the database directory and the Foxx Application directory
VOLUME ["/var/lib/arangodb3", "/var/lib/arangodb3-apps"]

COPY entrypoint-ubi.sh /entrypoint.sh
RUN ["chmod", "+x", "/entrypoint.sh"]
ENTRYPOINT [ "/entrypoint.sh" ]

EXPOSE 8529
CMD ["arangod"]
