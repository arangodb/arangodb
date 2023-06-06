We want images to be tagged based on the version of the base images as well was the versions of the most important components.
For example `arangodb/build-alpine-x86_64:3.16-gcc11.2-openssl3.1.1`
That image is based on Alpine 3.16 and has gcc 11.2 and OpenSSL 3.1.1 installed.

To automate and simplify this the build-image.sh script creates the image with the `latest` tag and afterwards automatically
determines the relevant version numbers, tags the image accordingly and prints the command to push the image.

