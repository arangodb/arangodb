FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arangodb.com
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y wget git python3 python3-pytest python3-pytest-asyncio python3-multidict python3-aiohttp python3-aiofiles && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists
RUN cd /tmp/; wget https://raw.githubusercontent.com/arangodb/python-arango/refs/heads/main/docs/requirements.txt && \
    pip3 install --break-system-packages -r requirements.txt && rm requirements.txt \
    wget https://raw.githubusercontent.com/arangodb/python-arango-async/refs/heads/main/docs/requirements.txt && \
    pip3 install --break-system-packages -r requirements.txt && rm requirements.txt
