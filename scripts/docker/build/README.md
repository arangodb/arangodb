This folder is intended to have resources to build docker images necessary for CI
=================================================================================

- The folder named `linux` is the Ubuntu-based docker build image (Linux docker mutiarch manifest based on x86-64 and arm64v8 images)
- The folder named `windows` is the Windows-base docker build image (Windows docker x86-64 image)

Linux manifest and images requirements
--------------------------------------

- The main devel build image is always `docker.io/arangodb/ubuntubuildarangodb-devel:latest` manifest
  - It's based on `arangodb/ubuntubuildarangodb-devel:latest(-amd64 | arm64v8)` images
  - In order to make a change a PR tag (based on `git rev-parse --short HEAD` or another unique hash) should be used instead of `latest` first
  - `linux/README.md` contains the guide on how to build and push images and a manifest
  - `docker.io/arangodb/ubuntubuildarangodb-devel:<TAG>` manifest should be used as a parameter for CI (CircleCI by default, see https://circleci.com/docs/selecting-a-workflow-to-run-using-pipeline-parameters/) to check changes
  - If a PR is ready to be merged one can build a `latest` images and manifest at the time of merge

Windows image requirements
--------------------------

- TODO
