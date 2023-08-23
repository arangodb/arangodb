FROM ubuntu:22.04
MAINTAINER hackers@arangodb.com

RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 python3-pip 7zip gdb tzdata curl wget jq binutils gcc python3-dev && \
    apt-get autoremove -y --purge && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*
   
RUN pip install psutil py7zr

RUN apt-get remove -y gcc python3-dev && \
    apt-get clean -y

CMD [ "bash" ]
