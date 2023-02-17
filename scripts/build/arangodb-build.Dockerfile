FROM alpine:3.16
MAINTAINER hackers@arangodb.com

ARG ARCH "x86_64"

RUN apk --no-cache add bison flex make cmake g++ git linux-headers python3 curl clang lld bash gdb libexecinfo-dev libexecinfo libexecinfo-static openssh sccache

# we need only need perl for openssl installation and can later removed them again
RUN apk --no-cache add perl
COPY install-openssl.sh /tools/
RUN [ "/tools/install-openssl.sh", "1.1.1", "s" ]
ENV OPENSSL_ROOT_DIR=/opt/openssl-1.1.1
RUN apk --no-cache del perl

CMD [ "sh" ]
