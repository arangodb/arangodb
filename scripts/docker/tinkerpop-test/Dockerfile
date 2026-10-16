FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
LABEL org.opencontainers.image.authors=hackers@arango.ai
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y openjdk-21-jdk-headless git netcat-traditional && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists

ARG MVN_VER="3.9.16"
RUN cd /tmp/ && \
    wget https://dlcdn.apache.org/maven/maven-3/${MVN_VER}/binaries/apache-maven-${MVN_VER}-bin.tar.gz && \
    tar xf apache-maven-${MVN_VER}-bin.tar.gz -C /opt && \
    echo 'export M3_HOME=/opt/apache-maven-${MVN_VER}\nexport MAVEN_HOME=/opt/apache-maven-${MVN_VER}\nexport PATH=${M3_HOME}/bin:${PATH}\n' > /etc/profile.d/maven.sh && \
    chmod +x /etc/profile.d/maven.sh && \
    ln -s /opt/apache-maven-${MVN_VER}/bin/mvn /usr/bin/ && \
    rm -f apache-maven-${MVN_VER}-bin.tar.gz

RUN git clone -n --depth=1 --filter=tree:0 https://github.com/arangodb/arangodb-tinkerpop-provider && \
  cd arangodb-tinkerpop-provider && \
  git checkout && \
  find . -name 'pom.xml' | sort | xargs cat > /tmp/maven_cache_seed && \
  mvn dependency:go-offline -Dtest.graph.type=simple && \
  cd ..; rm -rf arangodb-tinkerpop-provider
