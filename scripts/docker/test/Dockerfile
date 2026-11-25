# OpenSSL FIPS provider for use-fips pipelines. Same version and checksum as
# build_fips in scripts/docker/build/linux/tools/install.sh (the build image,
# which provides it to the FIPS server images); built here because the
# infrastructure pipeline builds the test and build images independently.
FROM ubuntu:26.04 AS fips
RUN apt-get update && \
    apt-get install -y --no-install-recommends ca-certificates curl gcc libc6-dev make perl && \
    rm -rf /var/lib/apt/lists/*
RUN cd /tmp && \
    version=3.1.2 && \
    sha256=a0ce69b8b97ea6a35b96875235aa453b966ba3cba8af2de23657d8b6767d6539 && \
    curl -fsSL -o openssl-${version}.tar.gz https://www.openssl.org/source/old/3.1/openssl-${version}.tar.gz && \
    echo "${sha256}  openssl-${version}.tar.gz" | sha256sum -c - && \
    tar xzf openssl-${version}.tar.gz && \
    cd openssl-${version} && \
    ./config --prefix=/opt enable-fips && \
    make -j"$(nproc)" && \
    mkdir -p /opt/lib/ossl-modules && \
    cp providers/fips.so /opt/lib/ossl-modules/

FROM ubuntu:26.04
LABEL org.opencontainers.image.authors=hackers@arango.ai

ARG arch
ARG CLANG_VERSION=22

RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common net-tools gnupg2 wget bzip2 && \
    apt-get update && \
    apt-get upgrade -y

COPY ./llvm.sources ./

RUN cat llvm.sources >> /etc/apt/sources.list.d/ubuntu.sources

RUN wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 \
    7zip gdb tzdata curl jq binutils gcc \
    llvm-${CLANG_VERSION} libatomic1 net-tools \
    libc6 libstdc++6 \
    libomp-${CLANG_VERSION}-dev liblapack-dev libopenblas-dev gfortran wget sed \
    python3 python3-pip openssl && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

# FIPS provider set up the way --server.fips-enable expects it (fixed paths,
# see lib/Ssl/FipsFeature.cpp). Inert unless a USE_FIPS arangod is started
# with that option (use-fips pipelines).
COPY --from=fips /opt/lib/ossl-modules/fips.so /opt/lib/ossl-modules/fips.so
COPY fips-only.cnf /opt/ssl/fips-only.cnf
RUN openssl fipsinstall -module /opt/lib/ossl-modules/fips.so -out /opt/ssl/fipsmodule.cnf

RUN pip3 install awscli psutil py7zr --break-system-packages

RUN if [ "$arch" = "amd64" ]; then \
        VERSION=$(curl -Ls https://api.github.com/repos/prometheus/prometheus/releases/latest | jq ".tag_name" | xargs | cut -c2-) && \
        wget -qO- "https://github.com/prometheus/prometheus/releases/download/v${VERSION}/prometheus-$VERSION.linux-amd64.tar.gz" | \
        tar xvz "prometheus-$VERSION.linux-amd64"/promtool --strip-components=1 && \
        strip -s promtool && \
        mv promtool /usr/local/bin/promtool; \
    fi
COPY VERSIONS .
COPY ARANGO-VERSION .
RUN \
  if [ "$arch" = "amd64" ]; then \
    arch="amd64"; \
  else \
    arch="arm64"; \
  fi; \
RCLONE_REV="v$(grep RCLONE_VERSION VERSIONS|sed -e 's;RCLONE_VERSION ";;' -e 's;";;')"; \
GO_VERSION="golang-$(grep RCLONE_GO VERSIONS|sed -e 's;RCLONE_GO ";;' -e 's;";;')"; \
ARANGO_VERSION="$(grep -Eo '^[0-9]+\.[0-9]+' ARANGO-VERSION)"; \
echo "https://github.com/arangodb/rclone-arangodb/releases/download/${GO_VERSION}/${GO_VERSION}_${ARANGO_VERSION}_${RCLONE_REV}_rclone-arangodb-linux-$arch"; \
wget -O /sbin/rclone-arangodb "https://github.com/arangodb/rclone-arangodb/releases/download/${GO_VERSION}/${GO_VERSION}_${ARANGO_VERSION}_${RCLONE_REV}_rclone-arangodb-linux-$arch" && \
 chmod +x /sbin/rclone-arangodb

CMD [ "bash" ]
