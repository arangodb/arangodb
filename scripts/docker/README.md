===build fresh docker images for the CI===
- edit the docker container definitions in this folder
- make sure https://dlcdn.apache.org/maven/maven-3 still has the `VER` found in the java docker files
- run the circle-ci flow, dial `rebuild-test-docker-images` to `true`.
- open the `build-test-docker-images` job
- open the `x64-test-docker-image` sub-job
- open the `Build Docker Image` sub-job
- locate the new tag:
  ```
  + docker tag public.ecr.aws/b0b8h2r4/test-ubuntu:latest-amd64 public.ecr.aws/b0b8h2r4/test-ubuntu:24.04-12ffcf7bf-amd64
  ```
  Here the tag is `12ffcf7bf`, which now needs to be set as default in:
- edit .circleci/config.yml - `test-docker-image` swap the tag in the `default`
- edit .circleci/base_config.yml - `test-docker-image` swap the tag in the `default`
push the changes to the PR, merge the PR - done. 
