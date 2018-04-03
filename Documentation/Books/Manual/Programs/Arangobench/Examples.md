Arangobench Examples
====================

    arangobench

Starts Arangobench with the default user and server endpoint.

    --test-case version --requests 1000 --concurrency 1

Runs the 'version' test case with 1000 requests, without concurrency.

    --test-case document --requests 1000 --concurrency 2

Runs the 'document' test case with 2000 requests, with two concurrent threads.

    --test-case document --requests 1000 --concurrency 2 --async true

Runs the 'document' test case with 2000 requests, with concurrency 2,
with async requests.

    --test-case document --requests 1000 --concurrency 2 --batch-size 10

Runs the 'document' test case with 2000 requests, with concurrency 2,
using batch requests.
