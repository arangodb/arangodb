FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arangodb.com
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y curl git python3 python3-pytest && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists && \
    curl https://raw.githubusercontent.com/arangodb/python-arango/refs/heads/main/docs/requirements.txt && \
    pip3 install -r requirements.txt && rm requirements.txt
