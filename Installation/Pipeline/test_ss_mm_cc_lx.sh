#!/bin/bash

concurrency=$1

chmod +x build/bin/*
chmod +x build/tests/*
chmod +x scripts/unittest
chmod +x scripts/disable-cores.sh
chmod +x build/tests/*

cat /proc/sys/kernel/core_pattern
ulimit -c

rm -f core.* *.log out
rm -Rf tmp && mkdir tmp
export TMPDIR=$(pwd)/tmp

build/bin/arangod --version
OPTS="--skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true"

# note that: shebang does not work if path contains a '@'

echo "
scripts/unittest boost                     --skipCache false 2>&1
scripts/unittest agency                    --minPort 9250 --maxPort 9259 $OPTS 2>&1
scripts/unittest arangobench               --minPort 9030 --maxPort 9039 $OPTS 2>&1
scripts/unittest arangosh                  --minPort 9020 --maxPort 9029 $OPTS --skipShebang true 2>&1
scripts/unittest authentication            --minPort 9040 --maxPort 9049 $OPTS  2>&1
scripts/unittest authentication_parameters --minPort 9050 --maxPort 9059 $OPTS 2>&1
scripts/unittest cluster_sync              --minPort 9060 --maxPort 9069 $OPTS 2>&1
scripts/unittest config                    --minPort 9070 --maxPort 9079 $OPTS 2>&1
scripts/unittest dfdb                      --minPort 9080 --maxPort 9089 $OPTS 2>&1
scripts/unittest dump                      --minPort 9230 --maxPort 9239 $OPTS 2>&1
scripts/unittest dump_authentication       --minPort 9090 --maxPort 9099 $OPTS 2>&1
scripts/unittest endpoints                 --minPort 9100 --maxPort 9109 $OPTS 2>&1
scripts/unittest http_replication          --minPort 9190 --maxPort 9199 $OPTS 2>&1
scripts/unittest http_server               --minPort 9120 --maxPort 9129 $OPTS  2>&1
scripts/unittest replication_ongoing       --minPort 9220 --maxPort 9229 $OPTS 2>&1
scripts/unittest replication_static        --minPort 9210 --maxPort 9219 $OPTS 2>&1
scripts/unittest replication_sync          --minPort 9200 --maxPort 9209 $OPTS 2>&1
scripts/unittest server_http               --minPort 9240 --maxPort 9249 $OPTS 2>&1
scripts/unittest shell_client              --minPort 9000 --maxPort 9009 $OPTS 2>&1
scripts/unittest shell_replication         --minPort 9180 --maxPort 9189 $OPTS 2>&1
scripts/unittest shell_server              --minPort 9010 --maxPort 9019 $OPTS 2>&1
scripts/unittest shell_server_aql          --minPort 9140 --maxPort 9149 $OPTS --testBuckets 4/0 2>&1
scripts/unittest shell_server_aql          --minPort 9150 --maxPort 9159 $OPTS --testBuckets 4/1 2>&1
scripts/unittest shell_server_aql          --minPort 9160 --maxPort 9169 $OPTS --testBuckets 4/2 2>&1
scripts/unittest shell_server_aql          --minPort 9170 --maxPort 9179 $OPTS --testBuckets 4/3 2>&1
scripts/unittest ssl_server                --minPort 9130 --maxPort 9139 $OPTS  2>&1
scripts/unittest upgrade                   --minPort 9110 --maxPort 9119 $OPTS 2>&1
" | parallel -j$concurrency
