parse-prometheus-text-format
==

A module that parses the [Prometheus text format](https://prometheus.io/docs/instrumenting/exposition_formats/#text-format-details). The output aims to be compatible with that of [prom2json](https://github.com/prometheus/prom2json). The code for parsing individual samples was ported from the [Prometheus Python Client](https://github.com/prometheus/client_python/blob/master/prometheus_client/parser.py).

Installation
==

```
npm install --save parse-prometheus-text-format
```

Usage
==

```js
import fs from 'fs';
import parsePrometheusTextFormat from 'parse-prometheus-text-format';

const metricsStr = fs.readFileSync('metrics.txt', 'utf8');
const parsed = parsePrometheusTextFormat(metricsStr);
```

Example
==

Input: 

```
# HELP jvm_classes_loaded The number of classes that are currently loaded in the JVM
# TYPE jvm_classes_loaded gauge
jvm_classes_loaded 3851.0

# HELP http_requests_total The total number of HTTP requests.
# TYPE http_requests_total counter
http_requests_total{method="post",code="200"} 1027 1395066363000
http_requests_total{method="post",code="400"}    3 1395066363000

# Escaping in label values:
msdos_file_access_time_seconds{path="C:\\DIR\\FILE.TXT",error="Cannot find file:\n\"FILE.TXT\""} 1.458255915e9

# Minimalistic line:
metric_without_timestamp_and_labels 12.47

# A weird metric from before the epoch:
something_weird{problem="division by zero"} +Inf -3982045

# A histogram, which has a pretty complex representation in the text format:
# HELP http_request_duration_seconds A histogram of the request duration.
# TYPE http_request_duration_seconds histogram
http_request_duration_seconds_bucket{le="0.05"} 24054
http_request_duration_seconds_bucket{le="0.1"} 33444
http_request_duration_seconds_bucket{le="0.2"} 100392
http_request_duration_seconds_bucket{le="0.5"} 129389
http_request_duration_seconds_bucket{le="1"} 133988
http_request_duration_seconds_bucket{le="+Inf"} 144320
http_request_duration_seconds_sum 53423
http_request_duration_seconds_count 144320

# Finally a summary, which has a complex representation, too:
# HELP rpc_duration_seconds A summary of the RPC duration in seconds.
# TYPE rpc_duration_seconds summary
rpc_duration_seconds{quantile="0.01"} 3102
rpc_duration_seconds{quantile="0.05"} 3272
rpc_duration_seconds{quantile="0.5"} 4773
rpc_duration_seconds{quantile="0.9"} 9001
rpc_duration_seconds{quantile="0.99"} 76656
rpc_duration_seconds_sum 1.7560473e+07
rpc_duration_seconds_count 2693

# HELP jvm_gc_collection_seconds Time spent in a given JVM garbage collector in seconds.
# TYPE jvm_gc_collection_seconds summary
jvm_gc_collection_seconds_count{gc="Copy",} 104.0
jvm_gc_collection_seconds_sum{gc="Copy",} 0.491
jvm_gc_collection_seconds_count{gc="MarkSweepCompact",} 12.0
jvm_gc_collection_seconds_sum{gc="MarkSweepCompact",} 0.486
```

Stringified result:

```js
[
    {
        "name": "jvm_classes_loaded",
        "help": "The number of classes that are currently loaded in the JVM",
        "type": "GAUGE",
        "metrics": [
            {
                "value": "3851.0"
            }
        ]
    },
    {
        "name": "http_requests_total",
        "help": "The total number of HTTP requests.",
        "type": "COUNTER",
        "metrics": [
            {
                "value": "1027",
                "labels": {
                    "method": "post",
                    "code": "200"
                }
            },
            {
                "value": "3",
                "labels": {
                    "method": "post",
                    "code": "400"
                }
            }
        ]
    },
    {
        "name": "msdos_file_access_time_seconds",
        "help": "",
        "type": "UNTYPED",
        "metrics": [
            {
                "value": "1.458255915e9",
                "labels": {
                    "path": "C:\\DIR\\FILE.TXT",
                    "error": "Cannot find file:\n\"FILE.TXT\""
                }
            }
        ]
    },
    {
        "name": "metric_without_timestamp_and_labels",
        "help": "",
        "type": "UNTYPED",
        "metrics": [
            {
                "value": "12.47"
            }
        ]
    },
    {
        "name": "something_weird",
        "help": "",
        "type": "UNTYPED",
        "metrics": [
            {
                "value": "+Inf",
                "labels": {
                    "problem": "division by zero"
                }
            }
        ]
    },
    {
        "name": "http_request_duration_seconds",
        "help": "A histogram of the request duration.",
        "type": "HISTOGRAM",
        "metrics": [
            {
                "buckets": {
                    "1": "133988",
                    "0.05": "24054",
                    "0.1": "33444",
                    "0.2": "100392",
                    "0.5": "129389",
                    "+Inf": "144320"
                },
                "count": "144320",
                "sum": "53423"
            }
        ]
    },
    {
        "name": "rpc_duration_seconds",
        "help": "A summary of the RPC duration in seconds.",
        "type": "SUMMARY",
        "metrics": [
            {
                "quantiles": {
                    "0.01": "3102",
                    "0.05": "3272",
                    "0.5": "4773",
                    "0.9": "9001",
                    "0.99": "76656"
                },
                "count": "2693",
                "sum": "1.7560473e+07"
            }
        ]
    },
    {
        "name": "jvm_gc_collection_seconds",
        "help": "Time spent in a given JVM garbage collector in seconds.",
        "type": "SUMMARY",
        "metrics": [
            {
                "labels": {
                    "gc": "Copy"
                },
                "count": "104.0",
                "sum": "0.491"
            },
            {
                "labels": {
                    "gc": "MarkSweepCompact"
                },
                "count": "12.0",
                "sum": "0.486"
            }
        ]
    }
]
```