set TESTS ""

set TESTS "$TESTS""250,runClusterTest1 replication2_server -\n"
set TESTS "$TESTS""250,runClusterTest1 replication2_client -\n"
set TESTS "$TESTS""250,runClusterTest1 auto - --test tests/js/common/shell/shell-replicated-logs-cluster.js\n"
set TESTS "$TESTS""250,runClusterTest1 auto - --test tests/js/common/shell/shell-prototype-state-cluster.js\n"
