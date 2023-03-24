FROM alpine:3.16
MAINTAINER hackers@arangodb.com

ARG ARCH "x86_64"

RUN apk --no-cache add bison flex make cmake g++ git linux-headers python3 curl clang lld bash gdb libexecinfo-dev libexecinfo libexecinfo-static openssh sccache groff nodejs npm

# we need only need perl for openssl installation and can later removed them again
RUN apk --no-cache add perl
COPY install-openssl.sh /tools/
RUN [ "/tools/install-openssl.sh", "3.0", ".8" ]
ENV OPENSSL_ROOT_DIR=/opt/openssl-3.0

COPY install-openldap.sh /tools/
RUN [ "/tools/install-openldap.sh", "3.0.8" ]
RUN apk --no-cache del perl

RUN ln /usr/bin/sccache /usr/local/bin/gcc && \
    ln /usr/bin/sccache /usr/local/bin/g++ && \
    ln /usr/bin/sccache /usr/local/bin/clang-14 && \
    ln /usr/bin/sccache /usr/local/bin/clang++-14

CMD [ "sh" ]
