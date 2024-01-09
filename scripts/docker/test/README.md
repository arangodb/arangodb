We want test images to be tagged based on the version of the base image as well as the versions of the most important components. In order to not break the base branch please add a random part with a branch-name dependency: i.e. `git rev-parse --short HEAD`.

For example `arangodb/test-ubuntu:<RAND>-amd64`
That image is based on Ubuntu 22.04 and has all necessary tools for x86-64 architecture in order to run tests.

To automate and simplify this, for each dockerfile there are build-image_*.sh scripts that create an image with the `latest-<ARCH>` tag and afterwards automatically determine the relevant version numbers, tags the image accordingly, and print the command to push the image.

Each supported architecture (x86-64 and arm64) has its own build-image_*.sh script.

Please take into account that cross-platform docker image build is not possible by default and there might be a need to use a native build machine.

After building and pushing images for each supported archirecture please create multiarch docker manifests based on tag name (IMAGE_TAG environment variable must be set) without architecture part: i.e. `abcdefgh1234` for `abcdefgh1234-amd64` and `abcdefgh1234-arm64`. build-manifest.sh script was made to automate manifest creation.

After building and pushing manifests, don't forget to update .circleci/base_config.yml so that the new manifest (a multiarch image) is actually used.
