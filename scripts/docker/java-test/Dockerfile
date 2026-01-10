FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arango.ai
 
ARG arch

RUN apt-get update --fix-missing && \
    apt-get install -y openjdk-21-jdk-headless git netcat-traditional && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists && \
    cd /tmp/ && \
    wget https://dlcdn.apache.org/maven/maven-3/3.9.12/binaries/apache-maven-3.9.12-bin.tar.gz && \
    tar xf apache-maven-3.9.12-bin.tar.gz -C /opt && \
    echo 'export M3_HOME=/opt/apache-maven-3.9.12\nexport MAVEN_HOME=/opt/apache-maven-3.9.12\nexport PATH=${M3_HOME}/bin:${PATH}\n' > /etc/profile.d/maven.sh && \
    chmod +x /etc/profile.d/maven.sh && \
    ln -s /opt/apache-maven-3.9.12/bin/mvn /usr/bin/ && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists /tmp/*gz



RUN git clone -n --depth=1 --filter=tree:0 https://github.com/arangodb/arangodb-java-driver && \
  cd arangodb-java-driver && \
  git checkout && \
  find . -name 'pom.xml' | sort | xargs cat > /tmp/maven_cache_seed && \
  mvn test-compile -am -pl test-functional && \
  cd ..; rm -rf arangodb-java-driver
  
