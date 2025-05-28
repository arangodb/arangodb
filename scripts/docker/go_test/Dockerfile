FROM arangodb/ubuntubuildarangodb-devel:15
MAINTAINER hackers@arangodb.com
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y golang && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists && \
    go install github.com/go-delve/delve/cmd/dlv@latest \
    
