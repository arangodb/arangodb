#!/bin/bash

concurrency=$1

echo "ARANGOD VERSION: `build/bin/arangod --version`"
echo "CORE PATTERN: `cat /proc/sys/kernel/core_pattern`"
echo "CORE LIMIT: `ulimit -c`"

rm -rf core.* *.log out
rm -rf tmp && mkdir tmp
export TMPDIR=$(pwd)/tmp
export TEMPDIR=$(pwd)/tmp

PORT01=`./Installation/Pipeline/port.sh`
PORT02=`./Installation/Pipeline/port.sh`
PORT03=`./Installation/Pipeline/port.sh`
PORT04=`./Installation/Pipeline/port.sh`
PORT05=`./Installation/Pipeline/port.sh`

trap "./Installation/Pipeline/port.sh --clean $PORT01 $PORT02 $PORT03 $PORT04 $PORT05" EXIT

PORTLDAP=`expr $PORT05 + 199`

docker rm -f ldap-$JOB_BASE_NAME || echo
mkdir -p ldap
docker pull arangodb/openldap-test-container:jessie
docker run -d -e LDAP_CERT_CN=127.0.0.1 --rm -p $PORTLDAP:389 -v $(pwd)/ldap:/cert --name ldap-$PORTLDAP arangodb/openldap-test-container:jessie

# note that: shebang does not work if path contains a '@'

OPTS="--storageEngine rocksdb --skipNondeterministic true --skipTimeCritical true  --configDir etc/jenkins --skipLogAnalysis true"

echo "
scripts/unittest agency                    --minPort `expr $PORT01 +   0` --maxPort `expr $PORT01 +  39` $OPTS 2>&1
scripts/unittest arangobench               --minPort `expr $PORT01 +  40` --maxPort `expr $PORT01 +  79` $OPTS --cluster true 2>&1
scripts/unittest arangosh                  --minPort `expr $PORT01 +  80` --maxPort `expr $PORT01 + 119` $OPTS --cluster true --skipShebang true 2>&1
scripts/unittest authentication            --minPort `expr $PORT01 + 120` --maxPort `expr $PORT01 + 159` $OPTS --cluster true 2>&1
scripts/unittest authentication_parameters --minPort `expr $PORT01 + 160` --maxPort `expr $PORT01 + 199` $OPTS --cluster true 2>&1
scripts/unittest config                    --minPort `expr $PORT02 +   0` --maxPort `expr $PORT02 +  39` $OPTS --cluster true 2>&1
scripts/unittest dfdb                      --minPort `expr $PORT02 +  40` --maxPort `expr $PORT02 +  79` $OPTS --cluster true 2>&1
scripts/unittest dump                      --minPort `expr $PORT02 +  80` --maxPort `expr $PORT02 + 119` $OPTS --cluster true 2>&1
scripts/unittest dump_authentication       --minPort `expr $PORT02 + 120` --maxPort `expr $PORT02 + 159` $OPTS --cluster true 2>&1
scripts/unittest endpoints                 --minPort `expr $PORT02 + 160` --maxPort `expr $PORT02 + 199` $OPTS --cluster true 2>&1
scripts/unittest http_server               --minPort `expr $PORT03 +   0` --maxPort `expr $PORT03 +  39` $OPTS --cluster true 2>&1
scripts/unittest ldap                      --minPort `expr $PORT03 +  40` --maxPort `expr $PORT03 +  79` $OPTS --cluster true --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort $PORTLDAP 2>&1
scripts/unittest server_http               --minPort `expr $PORT03 +  80` --maxPort `expr $PORT03 + 119` $OPTS --cluster true 2>&1
scripts/unittest shell_client              --minPort `expr $PORT03 + 120` --maxPort `expr $PORT03 + 159` $OPTS --cluster true 2>&1
scripts/unittest shell_server              --minPort `expr $PORT03 + 160` --maxPort `expr $PORT03 + 199` $OPTS --cluster true 2>&1
scripts/unittest shell_server_aql          --minPort `expr $PORT04 +   0` --maxPort `expr $PORT04 +  39` $OPTS --cluster true --testBuckets 4/0 2>&1
scripts/unittest shell_server_aql          --minPort `expr $PORT04 +  40` --maxPort `expr $PORT04 +  79` $OPTS --cluster true --testBuckets 4/1 2>&1
scripts/unittest shell_server_aql          --minPort `expr $PORT04 +  80` --maxPort `expr $PORT04 + 119` $OPTS --cluster true --testBuckets 4/2 2>&1
scripts/unittest shell_server_aql          --minPort `expr $PORT04 + 120` --maxPort `expr $PORT04 + 159` $OPTS --cluster true --testBuckets 4/3 2>&1
scripts/unittest ssl_server                --minPort `expr $PORT04 + 160` --maxPort `expr $PORT04 + 199` $OPTS --cluster true 2>&1
scripts/unittest upgrade                   --minPort `expr $PORT05 +   0` --maxPort `expr $PORT05 +  39` $OPTS --cluster true 2>&1
" | parallel --output-as-file --results log-output --no-notice --load 10 --jobs $concurrency
