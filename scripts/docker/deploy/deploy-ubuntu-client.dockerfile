# Client-tools-only image (arangodb/client-tools-preview): the same
# install tree as the server image, pruned of the server bits (arangod,
# the starter, server-side JS) by the CI job that builds this image —
# mirroring the TAR.GZ client bundle's contents (build-targz.sh).
FROM ubuntu:26.04
MAINTAINER Max Neunhoeffer <hackers@arangodb.com>

ARG arch

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && apt-get autoremove -y && apt-get clean

ENV GLIBCXX_FORCE_NEW=1

COPY ./install-client/ /

CMD [ "arangosh" ]
