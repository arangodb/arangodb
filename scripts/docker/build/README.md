This folder is intended to have resources to build docker images necessary for CI
=================================================================================

- The folder named `linux` is the Ubuntu-based docker build image (Linux docker mutiarch manifest based on x86-64 and arm64v8 images)
- The folder named `windows` is the Windows-base docker build image (Windows docker x86-64 image)

Linux manifest and images requirements
--------------------------------------

- The main devel build image is always `docker.io/arangodb/ubuntubuildarangodb-devel:<TAG>` manifest
  - A `<TAG>` is a is a positive natural number
  - It's based on `arangodb/ubuntubuildarangodb-devel:<TAG>(-amd64 | -arm64v8)` images
  - In order to make a change a PR tag should be used instead: it must be current `<TAG>` value plus 1
  - `linux/README.md` contains the guide on how to build and push images and a manifest
  - `docker.io/arangodb/ubuntubuildarangodb-devel:<TAG>` manifest should be used as a parameter for CI (CircleCI by default, see https://circleci.com/docs/selecting-a-workflow-to-run-using-pipeline-parameters/) to check changes
  - If a PR is ready to be merged one can put a new `<TAG>` within `.circleci/base_config.yaml` prior to merge
  - For a better convenience it makes sense to do `./regctl image copy arangodb/ubuntubuildarangodb-devel:<TAG> arangodb/ubuntubuildarangodb-devel:latest`

Windows image requirements
--------------------------

- TODO
