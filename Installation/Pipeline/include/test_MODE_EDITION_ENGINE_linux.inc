concurrency="$1"
mode="$2"
edition="$3"
engine="$4"

type="test"

if [ "$mode" == singleserver ]; then
    type="${type}_${mode}"
elif [ "$mode" == cluster ]; then
    type="${type}_${mode}"
else
    echo "$0: unknown mode '$mode', expecting 'singleserver' or 'cluster'"
    exit 1
fi

if [ "$edition" == community ]; then
    type="${type}_${edition}"
elif [ "$edition" == enterprise ]; then
    type="${type}_${edition}"
else
    echo "$0: unknown edition '$edition', expecting 'community' or 'enterprise'"
    exit 1
fi

if [ "$engine" == mmfiles ]; then
    type="${type}_${engine}"
elif [ "$engine" == rocksdb ]; then
    type="${type}_${engine}"
else
    echo "$0: unknown engine '$engine', expecting 'mmfiles' or 'rocksdb'"
    exit 1
fi

type="${type}_linux"

. ./Installation/Pipeline/include/test_log_info.inc
. ./Installation/Pipeline/include/test_setup_tmp.inc
. ./Installation/Pipeline/include/test_setup_ports_and_ldap.inc

OPTS="--storageEngine $engine --skipNondeterministic true --skipTimeCritical true --configDir etc/jenkins --skipLogAnalysis true"

if [ "$mode" == singleserver ]; then
    if [ "$edition" == enterprise ]; then
        ENTERPRISE_TESTS="scripts/unittest ldap --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort $PORTLDAP --minPort `expr $PORT03 + 60` --maxPort `expr $PORT03 + 69` $OPTS 2>&1"
    fi

    echo "$type
    scripts/unittest boost                     --skipCache false                                                                      2>&1
    scripts/unittest agency                                       --minPort `expr $PORT01 +   0` --maxPort `expr $PORT01 +   9` $OPTS 2>&1
    scripts/unittest arangobench                                  --minPort `expr $PORT01 +  10` --maxPort `expr $PORT01 +  19` $OPTS 2>&1
    scripts/unittest arangosh                  --skipShebang true --minPort `expr $PORT01 +  20` --maxPort `expr $PORT01 +  29` $OPTS 2>&1
    scripts/unittest authentication                               --minPort `expr $PORT01 +  30` --maxPort `expr $PORT01 +  39` $OPTS 2>&1
    scripts/unittest authentication_parameters                    --minPort `expr $PORT01 +  40` --maxPort `expr $PORT01 +  49` $OPTS 2>&1
    scripts/unittest cluster_sync                                 --minPort `expr $PORT01 +  50` --maxPort `expr $PORT01 +  59` $OPTS 2>&1
    scripts/unittest config                                       --minPort `expr $PORT01 +  60` --maxPort `expr $PORT01 +  69` $OPTS 2>&1
    scripts/unittest dfdb                                         --minPort `expr $PORT01 +  70` --maxPort `expr $PORT01 +  79` $OPTS 2>&1
    scripts/unittest dump                                         --minPort `expr $PORT01 +  80` --maxPort `expr $PORT01 +  89` $OPTS 2>&1
    scripts/unittest dump_authentication                          --minPort `expr $PORT01 +  90` --maxPort `expr $PORT01 +  99` $OPTS 2>&1
    scripts/unittest endpoints                                    --minPort `expr $PORT02 + 100` --maxPort `expr $PORT02 + 109` $OPTS 2>&1
    scripts/unittest http_replication                             --minPort `expr $PORT02 + 110` --maxPort `expr $PORT02 + 119` $OPTS 2>&1
    scripts/unittest http_server                                  --minPort `expr $PORT02 + 120` --maxPort `expr $PORT02 + 129` $OPTS 2>&1
    scripts/unittest replication_ongoing                          --minPort `expr $PORT02 + 130` --maxPort `expr $PORT02 + 139` $OPTS 2>&1
    scripts/unittest replication_static                           --minPort `expr $PORT02 + 140` --maxPort `expr $PORT02 + 149` $OPTS 2>&1
    scripts/unittest replication_sync                             --minPort `expr $PORT02 + 150` --maxPort `expr $PORT02 + 159` $OPTS 2>&1
    scripts/unittest server_http                                  --minPort `expr $PORT02 + 160` --maxPort `expr $PORT02 + 169` $OPTS 2>&1
    scripts/unittest shell_client                                 --minPort `expr $PORT02 + 170` --maxPort `expr $PORT02 + 179` $OPTS 2>&1
    scripts/unittest shell_replication                            --minPort `expr $PORT02 + 180` --maxPort `expr $PORT02 + 189` $OPTS 2>&1
    scripts/unittest shell_server                                 --minPort `expr $PORT02 + 190` --maxPort `expr $PORT02 + 199` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/0  --minPort `expr $PORT03 +   0` --maxPort `expr $PORT03 +   9` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/1  --minPort `expr $PORT03 +  10` --maxPort `expr $PORT03 +  19` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/2  --minPort `expr $PORT03 +  20` --maxPort `expr $PORT03 +  29` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/3  --minPort `expr $PORT03 +  30` --maxPort `expr $PORT03 +  39` $OPTS 2>&1
    scripts/unittest ssl_server                                   --minPort `expr $PORT03 +  40` --maxPort `expr $PORT03 +  49` $OPTS 2>&1
    scripts/unittest upgrade                                      --minPort `expr $PORT03 +  50` --maxPort `expr $PORT03 +  59` $OPTS 2>&1
    $ENTERPRISE_TESTS
    " | parallel --header 1 --results log-output --files --no-notice --load 10 --jobs $concurrency > log-output/${type}.log

elif [ "$mode" == cluster ]; then
    OPTS="$OPTS --cluster true"

    if [ "$edition" == enterprise ]; then
        ENTERPRISE_TESTS="scripts/unittest ldap --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort $PORTLDAP --minPort `expr $PORT04 + 120` --maxPort `expr $PORT04 + 159` $OPTS 2>&1"
    fi

    echo "$type
    scripts/unittest arangobench                                  --minPort `expr $PORT01 +   0` --maxPort `expr $PORT01 +  39` $OPTS 2>&1
    scripts/unittest arangosh                  --skipShebang true --minPort `expr $PORT01 +  40` --maxPort `expr $PORT01 +  79` $OPTS 2>&1
    scripts/unittest authentication                               --minPort `expr $PORT01 +  80` --maxPort `expr $PORT01 + 119` $OPTS 2>&1
    scripts/unittest authentication_parameters                    --minPort `expr $PORT01 + 120` --maxPort `expr $PORT01 + 159` $OPTS 2>&1
    scripts/unittest config                                       --minPort `expr $PORT01 + 160` --maxPort `expr $PORT01 + 199` $OPTS 2>&1
    scripts/unittest dump                                         --minPort `expr $PORT02 +   0` --maxPort `expr $PORT02 +  39` $OPTS 2>&1
    scripts/unittest dump_authentication                          --minPort `expr $PORT02 +  40` --maxPort `expr $PORT02 +  79` $OPTS 2>&1
    scripts/unittest endpoints                                    --minPort `expr $PORT02 +  80` --maxPort `expr $PORT02 + 119` $OPTS 2>&1
    scripts/unittest http_server                                  --minPort `expr $PORT02 + 120` --maxPort `expr $PORT02 + 159` $OPTS 2>&1
    scripts/unittest server_http                                  --minPort `expr $PORT02 + 160` --maxPort `expr $PORT02 + 199` $OPTS 2>&1
    scripts/unittest shell_client                                 --minPort `expr $PORT03 +   0` --maxPort `expr $PORT03 +  39` $OPTS 2>&1
    scripts/unittest shell_server                                 --minPort `expr $PORT03 +  40` --maxPort `expr $PORT03 +  79` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/0  --minPort `expr $PORT03 +  80` --maxPort `expr $PORT03 + 119` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/1  --minPort `expr $PORT03 + 120` --maxPort `expr $PORT03 + 159` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/2  --minPort `expr $PORT03 + 160` --maxPort `expr $PORT03 + 199` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/3  --minPort `expr $PORT04 +   0` --maxPort `expr $PORT04 +  39` $OPTS 2>&1
    scripts/unittest ssl_server                                   --minPort `expr $PORT04 +  40` --maxPort `expr $PORT04 +  79` $OPTS 2>&1
    scripts/unittest upgrade                                      --minPort `expr $PORT04 +  80` --maxPort `expr $PORT04 + 119` $OPTS 2>&1
    $ENTERPRISE_TESTS
    " | parallel --header 1 --results log-output --files --no-notice --load 10 --jobs $concurrency > log-output/${type}.log
fi

. ./Installation/Pipeline/include/test_check_result.inc $?
