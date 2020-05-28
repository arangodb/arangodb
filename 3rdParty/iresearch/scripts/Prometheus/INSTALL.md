PythonBenchmark.py script is used to upload IResearch benchmark data to Prometheus.
Upload is done throug pushing data to Prometheus PushGateway. Script requires Python 3.8 or above.
Pushgateway need to be installed using Pip command: pip install prometheus_client

Script run parameters:
python PythonBenchmark.py <Path-to-benchmark-logs> <Platform-used-to-run-benchmark> <Branch-user-to-run-benchmark> <Push-gate-url> <Job-name> 

Sample call: python PythonBenchmark.py C:\Data Windows10 master localhost:9091 benchmark

Following metrics are exposed to prometheus:
Time                 Execution time (microseconds)
Memory               Consumed memory (kbytes)
CPU                  CPU utilization % (across all cores, so values more than 100 are expected)
Wall_Clock           Elapsed wall clock (seconds)
MinorPageFaults      Minor (reclaiming a frame) page faults
MajorPageFaults      Major (requiring I/O) page faults
VolContextSwitches   Voluntary context switches
InvolContextSwitches Involuntary context switches

Each metrics comes with folllwong labels (althrough non-applicapable labels contains <None>):
"engine" - Search engine used. IResearch, Lucene. Determined while parsing benchmark logs.
"size"   - Search dataset size. Determined while parsing benchmark logs.
"category" - Benchmarked operation. Determined while parsing benchmark logs.
"repeat"  - number of test repeats. Determined while parsing benchmark logs.
"threads" - number of search/index threads used. Determined while parsing benchmark logs.
"random"  - random mode. Determined while parsing benchmark logs.
"scorer"  - scorer used. Determined while parsing benchmark logs.
"scorerarg" - scorer-arg used. Determined while parsing benchmark logs.
"run" - run number (Each test might be run several times to calculate average results)
"calls" - number of benchmark time measured
"branch" - Git Branch. Taken from <Branch-user-to-run-benchmark>
"platform" - Platform. Taken from <Platform-used-to-run-benchmark> argument
"stage" - Query Execution stage.

In order to configure uploading IResearch benchmark results to prometheus it is needed:

1. Install Prometheus PushGate.
  Docker image prom/pushgateway could be used.
2. Install and configre Prometheus. 
  Docker image prom/prometheus could be used.
  PushGate from step 1 should be added as scrape target.
  ------ From prometheus.yml -----
  scrape_configs:
  - job_name: pushgateway
  honor_labels: true
  scrape_interval: 3600s
  scrape_timeout: 10s
  metrics_path: /metrics
  scheme: http
  static_configs:
  - targets:
    - <Pushgate Host>:<Pushgate port>
  -------- End sample ---------------

3. Optionally Graphana could be configured.
	See Dashboard.json sample for pre-configured dashboard.
  To add annotations:
  3.1 Following snipped could be used in Jenkins job:
  cat << EOF > EventAnnotation.txt
  {
    "text": "Build: ${BUILD_NUMBER} | Commit: ${COMMIT_ID}",
    "tags": [ "BUILD" ]
  }
  EOF
  curl -s -X POST http://<Graphana-host>/api/annotations \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer ${GRAFANA_API_KEY}" \
    --data @EventAnnotation.txt

  3.2 Annotation for tag BUILD should be added to dashboard (included in sample pre-configured dashboard).