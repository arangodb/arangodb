# ann-benchmarks harness for ArangoDB's vector index. Bakes the pinned fork and
# its Python deps so a run needs no network beyond arangod + dataset downloads.
#
#   docker build -t ann-bench-harness .
#   docker run --rm --network host -v "$PWD/ann-output:/out" ann-bench-harness
#
# arangod is not part of this image; point ARANGO_HOST/ARANGO_PORT at one.
FROM python:3.11-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends git curl jq \
 && rm -rf /var/lib/apt/lists/*

ENV ANN_WORKDIR=/work \
    ANN_OUTPUT_DIR=/out \
    ANN_DATASET_CACHE=/data

WORKDIR /app
COPY run_ann_benchmark.sh load_run.py ./
RUN ./run_ann_benchmark.sh setup

VOLUME ["/out", "/data"]
ENTRYPOINT ["/app/run_ann_benchmark.sh"]
