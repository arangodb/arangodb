FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arangodb.com
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y openjdk-21-jdk-headless && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists 
    
