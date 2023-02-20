To build a new docker image please use the following command (assuming this folder as your working directory):
`docker build -t arangodb/build-alpine-x86_64:<alpine-version>-gcc<gcc-version>-openssl<openssl-version> --file arangodb-build.Dockerfile .`

For example:
`docker build -t arangodb/build-alpine-x86_64:3.16-gcc11.2-openssl1.1.1t --file arangodb-build.Dockerfile .`

After build we can push the new image to docker hub (assuming you have the necessary permissions):
`docker push arangodb/build-alpine-x86_64:3.16-gcc11.2-openssl1.1.1t`