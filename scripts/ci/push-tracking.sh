# Sourced by CI image-build jobs (via $BASH_ENV) to track exactly which images
# THIS run pushes, so cleanup only ever deletes images we created ourselves and
# never a pre-existing/skipped one.
#
# Records are written to /tmp/workspace/pushed/<job>.txt as "repo tag" lines,
# one per pushed image, BEFORE the push. Arch jobs persist that dir to the
# workspace; the manifest job attaches it, so it can also clean up the arch
# images it depends on. A SIGTERM/SIGINT trap performs the same cleanup on
# manual cancellation (where on_fail steps do not run).

_pushed_file() {
  mkdir -p /tmp/workspace/pushed
  printf '/tmp/workspace/pushed/%s.txt' "${CIRCLE_JOB:-job}"
}

# record_push <full-image-ref>
#   e.g. record_push public.ecr.aws/b0b8h2r4/test-ubuntu-go:24.04-abc123-amd64
record_push() {
  local ref="$1" repo
  repo="${ref%%:*}"; repo="${repo#public.ecr.aws/b0b8h2r4/}"
  echo "${repo} ${ref##*:}" >> "$(_pushed_file)"
  docker push "$ref"
}

# record_manifest <image> <tag> — records the manifest tag, then builds+pushes it
record_manifest() {
  local img="$1" tag="$2"
  echo "${img#public.ecr.aws/b0b8h2r4/} ${tag}" >> "$(_pushed_file)"
  ./build-manifest.sh "$img" "$tag"
}

# Delete every image recorded under /tmp/workspace/pushed (this run's pushes).
# An optional glob (or $PUSHED_CLEANUP_SCOPE) limits this to one image set's
# record files.
delete_pushed_images() {
  local scope="${1:-${PUSHED_CLEANUP_SCOPE:-*}}" f r t
  for f in /tmp/workspace/pushed/${scope}.txt; do
    [ -e "$f" ] || continue
    while read -r r t; do
      [ -n "${r:-}" ] || continue
      echo "  deleting ${r}:${t}"
      aws ecr-public batch-delete-image --region us-east-1 \
        --repository-name "$r" --image-ids imageTag="$t" || true
    done < "$f"
  done
}

_cleanup_pushed_on_cancel() {
  echo "Job cancelled — removing images this run pushed:"
  delete_pushed_images
  exit 143
}

# Only arm the cancel trap in an interactive job shell, not at file-parse time
# in unrelated contexts. Sourcing this file (re)installs it for each step.
trap _cleanup_pushed_on_cancel TERM INT
