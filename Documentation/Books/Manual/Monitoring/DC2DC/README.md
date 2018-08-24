<!-- don't edit here, its from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Monitoring datacenter to datacenter replication

{% hint 'info' %}
This feature is only available in the
[**Enterprise Edition**](https://www.arangodb.com/why-arangodb/arangodb-enterprise/)
{% endhint %}

This section includes information related to the monitoring of the _datacenter
to datacenter replication_.

For a general introduction to the _datacenter to datacenter replication_, please
refer to the [Datacenter to datacenter replication](../../Architecture/DeploymentModes/DC2DC/README.md)
chapter.

## Metrics

_ArangoSync_ (_master_ & _worker_) provide metrics that can be used for monitoring
the _datacenter to datacenter replication_ solution. These metrics are available
using the following HTTPS endpoints:

- GET `/metrics`: Provides metrics in a format supported by Prometheus.
- GET `/metrics.json`: Provides the same metrics in JSON format.

Both endpoints include help information per metrics.

Note: Both endpoints require authentication. Besides the usual authentication methods
these endpoints are also accessible using a special bearer token specified using the `--monitoring.token`
command line option.

The Prometheus output (`/metrics`) looks like this:

```text
...
# HELP arangosync_master_worker_registrations Total number of registrations
# TYPE arangosync_master_worker_registrations counter
arangosync_master_worker_registrations 2
# HELP arangosync_master_worker_storage Number of times worker info is stored, loaded
# TYPE arangosync_master_worker_storage counter
arangosync_master_worker_storage{kind="",op="save",result="success"} 20
arangosync_master_worker_storage{kind="empty",op="load",result="success"} 1
...
```

The JSON output (`/metrics.json`) looks like this:

```json
{
  ...
  "arangosync_master_worker_registrations": {
    "help": "Total number of registrations",
    "type": "counter",
    "samples": [
      {
        "value": 2
      }
    ]
  },
  "arangosync_master_worker_storage": {
    "help": "Number of times worker info is stored, loaded",
    "type": "counter",
    "samples": [
      {
        "value": 8,
        "labels": {
          "kind": "",
          "op": "save",
          "result": "success"
        }
      },
      {
        "value": 1,
        "labels": {
          "kind": "empty",
          "op": "load",
          "result": "success"
        }
      }
    ]
  }
  ...
}
```

Hint: To get a list of a metrics and their help information, run:

```bash
alias jq='docker run --rm -i realguess/jq jq'
curl -sk -u "<user>:<password>" https://<syncmaster-IP>:8629/metrics.json | \
  jq 'with_entries({key: .key, value:.value.help})'
```
