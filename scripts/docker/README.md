===build fresh docker images for the CI===
- edit the docker container definitions in this folder
- make sure https://dlcdn.apache.org/maven/maven-3 still has the `VER` found in the java docker files
- run the infrastructure pipeline with the `rebuild-images` parameter set to
  `test` (test runner + driver images), `build` (Ubuntu compiler image), or
  `all` (both).
- the pipeline builds and pushes the images, then automatically commits the
  updated image tags back to the triggering branch (the `default` values in
  `.circleci/config.yml`, `.circleci/base_config.yml`, and — for the build
  image — `.circleci/nightly_packages.yml`). No manual tag editing is needed.
- images already built for the current commit are skipped, so re-running the
  pipeline on the same commit is a no-op.

push the PR, merge it - done.
