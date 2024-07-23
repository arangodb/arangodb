FROM alpine:3.17
MAINTAINER hackers@arangodb.chmod

ARG arch

RUN apk add --no-cache pwgen nodejs numactl numactl-tools
RUN apk add --no-cache npm && npm install -g foxx-cli && apk del npm

COPY ./install/ /
COPY setup.sh /setup.sh
RUN /setup.sh && rm /setup.sh

# Adjust TZ by default since tzdata package isn't present (BTS-913)
RUN echo "UTC" > /etc/timezone

# The following is magic for unholy OpenShift security business.
# Containers in OpenShift by default run with a random UID but with GID 0,
# and we want that they can access the database and doc directories even
# without a volume mount:
RUN chgrp 0 /var/lib/arangodb3 /var/lib/arangodb3-apps && \
    chmod 775 /var/lib/arangodb3 /var/lib/arangodb3-apps

COPY entrypoint.sh /entrypoint.sh
RUN ["chmod", "+x", "/entrypoint.sh"]
ENTRYPOINT [ "/entrypoint.sh" ]

EXPOSE 8529
CMD [ "arangod" ]
