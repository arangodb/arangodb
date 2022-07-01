set TESTS ""

set TESTS "$TESTS""250,runSingleTest1 gtest_replication2 -\n"
set TESTS "$TESTS""250,runClusterTest1 replication2_server - --dumpAgencyOnError true\n"
set TESTS "$TESTS""250,runClusterTest1 replication2_client - --dumpAgencyOnError true\n"
set TESTS "$TESTS""250,runClusterTest1 auto - --dumpAgencyOnError true --test tests/js/common/shell/shell-replicated-logs-cluster.js\n"
set TESTS "$TESTS""250,runClusterTest1 auto - --dumpAgencyOnError true --test tests/js/common/shell/shell-prototype-state-cluster.js\n"
set TESTS "$TESTS""250,runClusterTest1 auto - --dumpAgencyOnError true --test tests/js/client/shell/transaction/shell-transaction.js\n"
