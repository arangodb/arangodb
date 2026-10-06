# Builds the Ubuntu-based compiler Docker image used by ArangoDB CI.
#
# Image name: public.ecr.aws/b0b8h2r4/arangodb/ubuntubuildarangodb-<MAJOR><MINOR>
#             e.g. ubuntubuildarangodb-312 for ArangoDB 3.12, ubuntubuildarangodb-40 for 4.0
# Tag format: <ubuntu-version>-<git-short-sha>  e.g. 24.04-abc1234
#             computed by determine-tag.sh after the image is built.
#
# ── Local usage (no push to production ECR) ───────────────────────────────────
#   make build-amd64                      # build amd64, tagged IMAGE:latest-amd64
#   make build-arm64                      # build arm64, tagged IMAGE:latest-arm64v8
#   make info                             # show resolved IMAGE name and current TAG
#
#   # Push to a custom registry (not production ECR):
#   make push-amd64 IMAGE=myregistry.example.com/ubuntubuildarangodb-312
#
# ── CI usage (called from .circleci/infrastructure.yml) ───────────────────────
#   make build-amd64 IMAGE=<ecr-image>
#   make push-amd64  IMAGE=<ecr-image> ALLOW_ECR_PUSH=1
#   make manifest    IMAGE=<ecr-image> TAG=<tag> ALLOW_ECR_PUSH=1

MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
REPO_ROOT    := $(realpath $(MAKEFILE_DIR)/../../../..)

# Derive MAJOR.MINOR from ARANGO-VERSION (e.g. "3.12.10-devel" → "312")
MAJOR_MINOR  := $(shell sed 's/\([0-9]*\)\.\([0-9]*\)\..*/\1\2/' \
                  "$(REPO_ROOT)/ARANGO-VERSION")

ECR_REGISTRY := public.ecr.aws/b0b8h2r4
ECR_IMAGE    := $(ECR_REGISTRY)/arangodb/ubuntubuildarangodb-$(MAJOR_MINOR)

# Default IMAGE is local (no registry prefix) — safe for local development.
# Override with IMAGE=<registry>/ubuntubuildarangodb-$(MAJOR_MINOR) to push elsewhere.
IMAGE        ?= ubuntubuildarangodb-$(MAJOR_MINOR)

# Set to 1 only in CI to allow pushing to the production ECR registry.
ALLOW_ECR_PUSH ?= 0

# TAG is auto-computed from the built amd64 image when not explicitly provided.
# Pass TAG=<value> explicitly for the manifest target if the image is not local
# (e.g. in the CI manifest job that runs after the per-arch build jobs).
TAG          ?= $(shell ./determine-tag.sh "$(IMAGE)" amd64 2>/dev/null)

.PHONY: all build build-amd64 build-arm64 push push-amd64 push-arm64 \
        manifest info help _check-push

help:
	@echo ""
	@echo "  ArangoDB Ubuntu build image — $(IMAGE)"
	@echo ""
	@echo "  Targets:"
	@echo "    build-amd64   Build amd64 image  → $(IMAGE):latest-amd64"
	@echo "    build-arm64   Build arm64 image  → $(IMAGE):latest-arm64v8"
	@echo "    build         Build both"
	@echo "    push-amd64    Tag + push amd64   → $(IMAGE):<TAG>-amd64"
	@echo "    push-arm64    Tag + push arm64   → $(IMAGE):<TAG>-arm64v8"
	@echo "    push          Push both"
	@echo "    manifest      Create + push multi-arch manifest"
	@echo "    all           build + push + manifest"
	@echo "    info          Show resolved image name and tag"
	@echo ""
	@echo "  Variables:"
	@echo "    IMAGE=$(IMAGE)"
	@echo "    ECR_IMAGE=$(ECR_IMAGE)  (production — CI only)"
	@echo "    ALLOW_ECR_PUSH=$(ALLOW_ECR_PUSH)  (set to 1 only in CI)"
	@echo ""

all: build push manifest

build: build-amd64 build-arm64

build-amd64:
	docker build --pull --no-cache \
	  --build-arg ARCH=x86_64 \
	  -t "$(IMAGE):latest-amd64" \
	  -f Dockerfile.x86-64 .

build-arm64:
	docker build --pull --no-cache \
	  --build-arg ARCH=aarch64 \
	  -t "$(IMAGE):latest-arm64v8" \
	  -f Dockerfile.arm64 .

info:
	@echo "IMAGE = $(IMAGE)"
	@echo "ECR   = $(ECR_IMAGE)"
	@if docker image inspect "$(IMAGE):latest-amd64" > /dev/null 2>&1; then \
	  echo "TAG   = $$(./determine-tag.sh "$(IMAGE)" amd64)"; \
	else \
	  echo "TAG   = (build first to compute)"; \
	fi

# Guard: block pushes to production ECR from local builds unless ALLOW_ECR_PUSH=1.
# Literal prefix match via case (the quoted pattern part is not a regex, so
# the dots in the registry host match only themselves).
_check-push:
	@case "$(IMAGE)" in \
	"$(ECR_REGISTRY)"*) \
	  if [ "$(ALLOW_ECR_PUSH)" != "1" ]; then \
	    echo ""; \
	    echo "  Pushing to the production ECR registry is not allowed from local builds."; \
	    echo "  Use a custom registry instead:"; \
	    echo "    make push IMAGE=<your-registry>/ubuntubuildarangodb-$(MAJOR_MINOR)"; \
	    echo "  In CI set ALLOW_ECR_PUSH=1."; \
	    echo ""; \
	    exit 1; \
	  fi ;; \
	esac

push: push-amd64 push-arm64

push-amd64: _check-push
	$(eval _TAG := $(shell ./determine-tag.sh "$(IMAGE)" amd64))
	@test -n "$(_TAG)" || (echo "ERROR: Could not compute tag — build the image first."; exit 1)
	docker tag "$(IMAGE):latest-amd64" "$(IMAGE):$(_TAG)-amd64"
	docker push "$(IMAGE):$(_TAG)-amd64"
	@echo "$(_TAG)"

push-arm64: _check-push
	$(eval _TAG := $(shell ./determine-tag.sh "$(IMAGE)" arm64v8))
	@test -n "$(_TAG)" || (echo "ERROR: Could not compute tag — build the image first."; exit 1)
	docker tag "$(IMAGE):latest-arm64v8" "$(IMAGE):$(_TAG)-arm64v8"
	docker push "$(IMAGE):$(_TAG)-arm64v8"
	@echo "$(_TAG)"

manifest: _check-push
	@test -n "$(TAG)" || \
	  (echo "ERROR: TAG is required for manifest."; \
	   echo "  Either build the amd64 image first (auto-computed) or pass TAG=<value>."; \
	   exit 1)
	./build-manifest.sh "$(IMAGE)" "$(TAG)"
