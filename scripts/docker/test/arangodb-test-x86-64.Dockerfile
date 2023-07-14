FROM ubuntu:22.04
MAINTAINER hackers@arangodb.com

ARG ARCH "x86_64"

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 python3-pip 7zip gdb tzdata curl wget jq binutils && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN VERSION=$(curl -Ls https://api.github.com/repos/prometheus/prometheus/releases/latest | jq ".tag_name" | xargs | cut -c2-) && \
    wget -qO- "https://github.com/prometheus/prometheus/releases/download/v${VERSION}/prometheus-$VERSION.linux-amd64.tar.gz" | \
    tar xvz "prometheus-$VERSION.linux-amd64"/promtool --strip-components=1 && \
    strip -s promtool && \
    mv promtool /usr/local/bin/promtool
RUN pip install psutil py7zr
ENV PROMTOOL_PATH=/usr/local/bin/

CMD [ "bash" ]
