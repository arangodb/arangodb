# Image arangodb/clang-format: a pinned clang-format, so that every developer
# formats code with the same version. Used by scripts/clang-format.sh ("format")
# and .githooks/pre-commit ("validate"). It is not built by the CI pipeline in
# scripts/docker/README.md; build and push it manually:
#   docker build -t arangodb/clang-format:<tag> scripts/docker/clang-format
FROM alpine:3.24

# Alpine drops superseded package revisions from its mirrors, so this exact pin
# makes a rebuild fail loudly instead of silently picking up another version.
ENV version=22.1.3-r2
LABEL description="ClangFormat ${version}"

WORKDIR /usr/src/

RUN apk add --no-cache git clang22-extra-tools=${version} \
 # git >= 2.35.2 refuses to work in a repository owned by another uid; the
 # repository is bind-mounted from the host, so its owner is not under our control.
 && git config --system --add safe.directory /usr/src/arangodb

COPY entrypoint.sh .
RUN chmod +x entrypoint.sh

ENTRYPOINT ["./entrypoint.sh"]
