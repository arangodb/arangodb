FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arangodb.com
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y openjdk-21-jdk-headless git maven netcat-traditional && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists 
    
RUN git clone -n --depth=1 --filter=tree:0 https://github.com/arangodb/arangodb-tinkerpop-provider && \
  cd arangodb-tinkerpop-provider && \
  git checkout && \
  find . -name 'pom.xml' | sort | xargs cat > /tmp/maven_cache_seed && \
  mvn dependency:go-offline -Dtest.graph.type=simple && \
  cd ..; rm -rf arangodb-tinkerpop-provider  
