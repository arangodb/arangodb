FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
LABEL org.opencontainers.image.authors=hackers@arango.ai
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y openjdk-17-jdk-headless git netcat-traditional && \
    apt-get update && \
    apt-get upgrade -y

RUN cd /tmp/ && \
    VER="3.9.16" \
    wget https://dlcdn.apache.org/maven/maven-3/${VER}/binaries/apache-maven-${VER}-bin.tar.gz && \
    tar xf apache-maven-${VER}-bin.tar.gz -C /opt && \
    echo 'export M3_HOME=/opt/apache-maven-${VER}\nexport MAVEN_HOME=/opt/apache-maven-${VER}\nexport PATH=${M3_HOME}/bin:${PATH}\n' > /etc/profile.d/maven.sh && \
    chmod +x /etc/profile.d/maven.sh && \
    ln -s /opt/apache-maven-${VER}/bin/mvn /usr/bin/ && \
    rm -f apache-maven-${VER}-bin.tar.gz

RUN git clone -n --depth=1 --filter=tree:0 https://github.com/arangodb/spring-data && \
  cd spring-data && \
  git checkout && \
  find . -name 'pom.xml' | sort | xargs cat > /tmp/maven_cache_seed && \
  mvn dependency:go-offline  -am -pl && \
  cd ..; rm -rf spring-data
