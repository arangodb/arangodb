FROM ubuntu:24.04
MAINTAINER hackers@arangodb.com

ARG arch
ARG CLANG_VERSION=19

RUN apt-get update --fix-missing && \
    apt-get install -y software-properties-common net-tools gnupg2 wget && \
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
    libomp-${CLANG_VERSION}-dev liblapack-dev libopenblas-dev gfortran wget \
    python3 python3-pip && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN pip3 install psutil py7zr --break-system-packages

RUN if [ "$arch" = "amd64" ]; then \
        VERSION=$(curl -Ls https://api.github.com/repos/prometheus/prometheus/releases/latest | jq ".tag_name" | xargs | cut -c2-) && \
        wget -qO- "https://github.com/prometheus/prometheus/releases/download/v${VERSION}/prometheus-$VERSION.linux-amd64.tar.gz" | \
        tar xvz "prometheus-$VERSION.linux-amd64"/promtool --strip-components=1 && \
        strip -s promtool && \
        mv promtool /usr/local/bin/promtool; \
    fi

RUN wget -O /sbin/rclone-arangodb https://github.com/arangodb/oskar/raw/master/rclone/v1.62.2/rclone-arangodb-linux-$arch && chmod +x /sbin/rclone-arangodb

CMD [ "bash" ]
