FROM ubuntu:22.04
MAINTAINER hackers@arangodb.com

ARG ARCH "x86_64"

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 python3-pip 7zip gdb && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN pip install psutil py7zr

CMD [ "bash" ]
