# ArangoDB Ubuntu Build Image

Ubuntu-based Docker image containing clang, v8 (pre-built under `/opt/v8`), and all
other tooling required to compile ArangoDB. Used by every compile job in CircleCI.

## Image naming

```
public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-<MAJOR><MINOR>:<TAG>
```

- `MAJOR` and `MINOR` are derived automatically from `ARANGO-VERSION` at the repo root —
  e.g. `3.12.x` → `ubuntubuildarangodb-312`, `4.0.x` → `ubuntubuildarangodb-40`.
- `TAG` is `<ubuntu-version>-<git-short-sha>` computed by `determine-tag.sh` after
  the image is built — e.g. `24.04-abc1234`.

Per-arch images are pushed as `<TAG>-amd64` and `<TAG>-arm64v8`; the multi-arch
manifest is pushed as `<TAG>`.

## Files

| File | Purpose |
|---|---|
| `Dockerfile.x86-64` | Dockerfile for x86\_64 / amd64 |
| `Dockerfile.arm64` | Dockerfile for arm64 |
| `determine-tag.sh` | Compute tag by running the built image to get its Ubuntu version + git SHA |
| `build-manifest.sh` | Create and push a multi-arch manifest |
| `Makefile` | Orchestrates local and CI builds (see below) |

## Local build (no push to production ECR)

Run from this directory on the target machine:

```bash
# Build amd64 image (on an x86_64 machine)
make build-amd64

# Build arm64 image (on an arm64 machine or via QEMU)
make build-arm64

# Inspect the resolved image name and computed tag
make info
```

The default `IMAGE` has no registry prefix (`ubuntubuildarangodb-312`) so images
stay in the local Docker daemon and are never pushed to production ECR.

To push to a **custom** (non-production) registry for sharing or testing:

```bash
make push-amd64 IMAGE=myregistry.example.com/ubuntubuildarangodb-312
make push-arm64 IMAGE=myregistry.example.com/ubuntubuildarangodb-312
make manifest   IMAGE=myregistry.example.com/ubuntubuildarangodb-312
```

Pushing directly to `public.ecr.aws/b0b8h2r4` is blocked by the Makefile unless
`ALLOW_ECR_PUSH=1` is explicitly set — that flag is reserved for CI.

## CI build (`.circleci/infrastructure.yml`)

The `build-ubuntu-build-images` workflow calls `make` with the full ECR image name
and `ALLOW_ECR_PUSH=1`:

```bash
# Per-arch job (amd64):
make build-amd64 IMAGE=public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-312
make push-amd64  IMAGE=public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-312 ALLOW_ECR_PUSH=1

# Manifest job (after both arches succeed):
make manifest    IMAGE=public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-312 \
                 TAG=24.04-abc1234 ALLOW_ECR_PUSH=1
```

After a successful CI run, the tag and the `build-docker-image` defaults in
`.circleci/config.yml`, `.circleci/base_config.yml`, and
`.circleci/nightly_packages.yml` are updated and committed back automatically.
