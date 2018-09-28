FROM ubuntu:18.04

USER root
   
RUN apt-get update && \
	apt-get upgrade -y && \
    apt-get install -y npm build-essential libjemalloc-dev cmake python2.7 libldap2-dev nano git && \
    npm install gitbook-cli -g

EXPOSE 8529

WORKDIR /data
