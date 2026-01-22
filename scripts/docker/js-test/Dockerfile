FROM public.ecr.aws/b0b8h2r4/test-ubuntu:latest
MAINTAINER hackers@arango.ai
 
ARG arch

ARG NODE_VERSION="20.14.0"
RUN apt-get update --fix-missing && \
    apt-get install -y curl git && \
    apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/cache/apt/archives /var/lib/apt/lists && \
    curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash && \
  export NVM_DIR="$HOME/.nvm" &&\
  [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" \
  [ -s "$NVM_DIR/bash_completion" ] && \. "$NVM_DIR/bash_completion" && \
  nvm install ${NODE_VERSION} && \
  nvm use ${NODE_VERSION} && \
  node --version && \
  npm install -g yarn && \
  ln -s /root/.nvm/versions/node/v${NODE_VERSION}/bin/* /usr/local/bin/


