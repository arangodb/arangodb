<!-- don't edit here, it's from https://@github.com/arangodb/arangosync.git / docs/Manual/ -->
# Prometheus & Grafana (optional)

_ArangoSync_ provides metrics in a format supported by [Prometheus](https://prometheus.io).
We also provide a standard set of dashboards for viewing those metrics in [Grafana](https://grafana.org).

If you want to use these tools, please refer to their websites for instructions
on how to deploy them.

After deployment, you must configure _Prometheus_ using a configuration file that
instructs it about which targets to scrape. For _ArangoSync_ you should configure
scrape targets for all _sync masters_ and all _sync workers_. To do so, you can
use a configuration such as this:

```text
global:
  scrape_interval:     10s # scrape targets every 10 seconds.

scrape_configs:
  # Scrap sync masters
  - job_name: 'sync_master'
    scheme: 'https'
    bearer_token: "${MONITORINGTOKEN}"
    tls_config:
      insecure_skip_verify: true
    static_configs:
      - targets:
        - "${IPMASTERA1}:8629"
        - "${IPMASTERA2}:8629"
        - "${IPMASTERB1}:8629"
        - "${IPMASTERB2}:8629"
        labels:
          type: "master"
    relabel_configs:
      - source_labels: [__address__]
        regex:         ${IPMASTERA1}\:8629|${IPMASTERA2}\:8629
        target_label:  dc
        replacement:   A
      - source_labels: [__address__]
        regex:         ${IPMASTERB1}\:8629|${IPMASTERB2}\:8629
        target_label:  dc
        replacement:   B
      - source_labels: [__address__]
        regex:         ${IPMASTERA1}\:8629|${IPMASTERB1}\:8629
        target_label:  instance
        replacement:   1
      - source_labels: [__address__]
        regex:         ${IPMASTERA2}\:8629|${IPMASTERB2}\:8629
        target_label:  instance
        replacement:   2

  # Scrap sync workers
  - job_name: 'sync_worker'
    scheme: 'https'
    bearer_token: "${MONITORINGTOKEN}"
    tls_config:
      insecure_skip_verify: true
    static_configs:
      - targets:
        - "${IPWORKERA1}:8729"
        - "${IPWORKERA2}:8729"
        - "${IPWORKERB1}:8729"
        - "${IPWORKERB2}:8729"
        labels:
          type: "worker"
    relabel_configs:
      - source_labels: [__address__]
        regex:         ${IPWORKERA1}\:8729|${IPWORKERA2}\:8729
        target_label:  dc
        replacement:   A
      - source_labels: [__address__]
        regex:         ${IPWORKERB1}\:8729|${IPWORKERB2}\:8729
        target_label:  dc
        replacement:   B
      - source_labels: [__address__]
        regex:         ${IPWORKERA1}\:8729|${IPWORKERB1}\:8729
        target_label:  instance
        replacement:   1
      - source_labels: [__address__]
        regex:         ${IPWORKERA2}\:8729|${IPWORKERB2}\:8729
        target_label:  instance
        replacement:   2
```

Note: The above example assumes 2 datacenters, with 2 _sync masters_ & 2 _sync workers_
per datacenter. You have to replace all `${...}` variables in the above configuration
with applicable values from your environment.

## Recommended deployment environment

_Prometheus_ can be a memory & CPU intensive process. It is recommended to keep them
on other machines than used to run the ArangoDB cluster or _ArangoSync_ components.

Consider these machines "cattle", unless you configure alerting on _prometheus_,
in which case it is recommended to consider these machines "pets".
