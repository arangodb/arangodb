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
OPTS="--storageEngine rocksdb --skipNondeterministic true --skipTimeCritical true --configDir etc/jenkins --skipLogAnalysis true"

# note that: shebang does not work if path contains a '@'

# removed for now because known to be unstable
# scripts/unittest resilience          $OPTS --minPort  9261 --maxPort  9300 --test js/server/tests/resilience/moving-shards-cluster.js 2>&1 | tee resilienceMove.log 

# start LDAP container
docker rm -f ldap-$JOB_BASE_NAME || echo
mkdir -p ldap
docker pull arangodb/openldap-test-container:jessie
docker run -d -e LDAP_CERT_CN=127.0.0.1 --rm -p 3892:389 -v $(pwd)/ldap:/cert --name ldap-$JOB_BASE_NAME arangodb/openldap-test-container:jessie

echo "
scripts/unittest agency                    $OPTS --minPort 25100 --maxPort 25139  2>&1
scripts/unittest arangobench               $OPTS --minPort 24460 --maxPort 24499  --cluster true 2>&1
scripts/unittest authentication            $OPTS --minPort 24500 --maxPort 24539  --cluster true 2>&1
scripts/unittest authentication_parameters $OPTS --minPort 24540 --maxPort 24579  --cluster true 2>&1
scripts/unittest config                    $OPTS --minPort 24580 --maxPort 24619  --cluster true  2>&1
scripts/unittest dfdb                      $OPTS --minPort 25060 --maxPort 25099  --cluster true 2>&1
scripts/unittest dump                      $OPTS --minPort 25020 --maxPort 25059  --cluster true 2>&1
scripts/unittest dump_authentication       $OPTS --minPort 24620 --maxPort 24659  --cluster true 2>&1
scripts/unittest endpoints                 $OPTS --minPort 24660 --maxPort 24699  --cluster true 2>&1
scripts/unittest http_server               $OPTS --minPort 24740 --maxPort 24779  --cluster true 2>&1
scripts/unittest ldap                      $OPTS --minPort 25140 --maxPort 25179  --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort 3892 2>&1
scripts/unittest resilience                $OPTS --minPort 24380 --maxPort 24419  --test js/server/tests/resilience/resilience-synchronous-repl-cluster.js 2>&1
scripts/unittest server_http               $OPTS --minPort 24980 --maxPort 25019  --cluster true 2>&1
scripts/unittest shell_client              $OPTS --minPort 24300 --maxPort 24339  --cluster true 2>&1
scripts/unittest shell_server              $OPTS --minPort 24340 --maxPort 24379  --cluster true 2>&1
scripts/unittest shell_server_aql          $OPTS --minPort 24820 --maxPort 24859  --cluster true --testBuckets 4/0 2>&1
scripts/unittest shell_server_aql          $OPTS --minPort 24860 --maxPort 24899  --cluster true --testBuckets 4/1 2>&1
scripts/unittest shell_server_aql          $OPTS --minPort 24900 --maxPort 24939  --cluster true --testBuckets 4/2 2>&1
scripts/unittest shell_server_aql          $OPTS --minPort 24940 --maxPort 24979  --cluster true --testBuckets 4/3 2>&1
scripts/unittest ssl_server                $OPTS --minPort 24780 --maxPort 24819  --cluster true 2>&1
scripts/unittest upgrade                   $OPTS --minPort 24700 --maxPort 24739  --cluster true 2>&1
" | parallel -j$concurrency
