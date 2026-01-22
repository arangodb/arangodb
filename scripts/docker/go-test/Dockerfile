FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arango.ai
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y golang git && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists && \
    go install github.com/go-delve/delve/cmd/dlv@latest
    
