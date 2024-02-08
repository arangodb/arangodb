FROM ubuntu:23.10
MAINTAINER hackers@arangodb.com

ARG arch

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 python3-pip 7zip gdb tzdata curl wget jq binutils gcc python3-dev llvm libatomic1 && \
    pip install psutil py7zr --break-system-packages && \
    apt-get remove -y python3-dev gcc && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN wget -O /sbin/rclone-arangodb https://github.com/arangodb/oskar/raw/master/rclone/v1.62.2/rclone-arangodb-linux-$arch && chmod +x /sbin/rclone-arangodb

CMD [ "bash" ]
