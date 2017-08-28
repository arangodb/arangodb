concurrency="$1"
mode="$2"
edition="$3"
engine="$4"
os="$5"

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

if [ "$os" == linux ]; then
    type="${type}_${os}"
elif [ "$os" == mac ]; then
    type="${type}_${os}"
else
    echo "$0: unknown engine '$os', expecting 'linux' or 'mac'"
    exit 1
fi

. ./Installation/Pipeline/include/test_ulimits.inc
. ./Installation/Pipeline/include/test_log_info.inc
. ./Installation/Pipeline/include/test_setup_tmp.inc
. ./Installation/Pipeline/include/test_setup_ports_and_ldap.inc

OPTS="--storageEngine $engine --skipNondeterministic true --skipTimeCritical true --configDir etc/jenkins --skipLogAnalysis true"

if [ "$mode" == singleserver ]; then
    if [ "$edition" == enterprise -a "$os" == linux ]; then
        ENTERPRISE_TESTS="scripts/unittest ldap --minPort `expr $PORT01 + 1000` --maxPort `expr $PORT01 + 1069` --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort $PORTLDAP $OPTS 2>&1"
    fi

    echo "$type
    scripts/unittest boost                     --skipCache false                                                                      2>&1
    scripts/unittest agency                                       --minPort `expr $PORT01 +   0` --maxPort `expr $PORT01 +   9` $OPTS 2>&1
    scripts/unittest resilience                                   --minPort `expr $PORT01 +  10` --maxPort `expr $PORT01 +  19` $OPTS 2>&1
    scripts/unittest arangobench                                  --minPort `expr $PORT01 +  20` --maxPort `expr $PORT01 +  29` $OPTS 2>&1
    scripts/unittest arangosh                  --skipShebang true --minPort `expr $PORT01 +  30` --maxPort `expr $PORT01 +  39` $OPTS 2>&1
    scripts/unittest authentication                               --minPort `expr $PORT01 +  40` --maxPort `expr $PORT01 +  49` $OPTS 2>&1
    scripts/unittest authentication_parameters                    --minPort `expr $PORT01 +  50` --maxPort `expr $PORT01 +  59` $OPTS 2>&1
    scripts/unittest authentication_parameters                    --minPort `expr $PORT01 +  60` --maxPort `expr $PORT01 +  69` $OPTS 2>&1
    scripts/unittest cluster_sync                                 --minPort `expr $PORT01 +  70` --maxPort `expr $PORT01 +  79` $OPTS 2>&1
    scripts/unittest config                                       --minPort `expr $PORT01 +  80` --maxPort `expr $PORT01 +  89` $OPTS 2>&1
    scripts/unittest dfdb                                         --minPort `expr $PORT01 +  90` --maxPort `expr $PORT01 +  99` $OPTS 2>&1
    scripts/unittest dump                                         --minPort `expr $PORT01 + 100` --maxPort `expr $PORT01 + 109` $OPTS 2>&1
    scripts/unittest dump_authentication                          --minPort `expr $PORT01 + 110` --maxPort `expr $PORT01 + 119` $OPTS 2>&1
    scripts/unittest endpoints                                    --minPort `expr $PORT01 + 120` --maxPort `expr $PORT01 + 129` $OPTS 2>&1
    scripts/unittest http_replication                             --minPort `expr $PORT01 + 130` --maxPort `expr $PORT01 + 139` $OPTS 2>&1
    scripts/unittest http_server                                  --minPort `expr $PORT01 + 140` --maxPort `expr $PORT01 + 149` $OPTS 2>&1
    scripts/unittest replication_ongoing                          --minPort `expr $PORT01 + 150` --maxPort `expr $PORT01 + 159` $OPTS 2>&1
    scripts/unittest replication_static                           --minPort `expr $PORT01 + 160` --maxPort `expr $PORT01 + 169` $OPTS 2>&1
    scripts/unittest replication_sync                             --minPort `expr $PORT01 + 170` --maxPort `expr $PORT01 + 179` $OPTS 2>&1
    scripts/unittest server_http                                  --minPort `expr $PORT01 + 180` --maxPort `expr $PORT01 + 189` $OPTS 2>&1
    scripts/unittest shell_client                                 --minPort `expr $PORT01 + 190` --maxPort `expr $PORT01 + 199` $OPTS 2>&1
    scripts/unittest shell_replication                            --minPort `expr $PORT01 + 200` --maxPort `expr $PORT01 + 209` $OPTS 2>&1
    scripts/unittest shell_server                                 --minPort `expr $PORT01 + 210` --maxPort `expr $PORT01 + 219` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/0  --minPort `expr $PORT01 + 220` --maxPort `expr $PORT01 + 229` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/1  --minPort `expr $PORT01 + 230` --maxPort `expr $PORT01 + 239` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/2  --minPort `expr $PORT01 + 240` --maxPort `expr $PORT01 + 249` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/3  --minPort `expr $PORT01 + 250` --maxPort `expr $PORT01 + 259` $OPTS 2>&1
    scripts/unittest ssl_server                                   --minPort `expr $PORT01 + 260` --maxPort `expr $PORT01 + 269` $OPTS 2>&1
    scripts/unittest upgrade                                      --minPort `expr $PORT01 + 270` --maxPort `expr $PORT01 + 279` $OPTS 2>&1
    scripts/unittest readOnly                                     --minPort `expr $PORT01 + 280` --maxPort `expr $PORT01 + 289` $OPTS 2>&1
    $ENTERPRISE_TESTS
    " | parallel --verbose --header 1 --results log-output --files --no-notice --load 10 --jobs $concurrency > log-output/tests.log

elif [ "$mode" == cluster ]; then
    OPTS="$OPTS --cluster true"

    if [ "$edition" == enterprise -a "$os" == linux ]; then
        ENTERPRISE_TESTS="scripts/unittest ldap --minPort `expr $PORT01 + 1000` --maxPort `expr $PORT01 + 1039` --caCertFilePath $(pwd)/ldap/ca_server.pem --ldapHost 127.0.0.1 --ldapPort $PORTLDAP $OPTS 2>&1"
    fi

    echo "$type
    scripts/unittest arangobench                                  --minPort `expr $PORT01 +   0` --maxPort `expr $PORT01 +  39` $OPTS 2>&1
    scripts/unittest arangosh                  --skipShebang true --minPort `expr $PORT01 +  40` --maxPort `expr $PORT01 +  79` $OPTS 2>&1
    scripts/unittest authentication                               --minPort `expr $PORT01 +  80` --maxPort `expr $PORT01 + 119` $OPTS 2>&1
    scripts/unittest authentication_parameters                    --minPort `expr $PORT01 + 120` --maxPort `expr $PORT01 + 159` $OPTS 2>&1
    scripts/unittest config                                       --minPort `expr $PORT01 + 160` --maxPort `expr $PORT01 + 199` $OPTS 2>&1
    scripts/unittest dump                                         --minPort `expr $PORT01 + 200` --maxPort `expr $PORT01 + 239` $OPTS 2>&1
    scripts/unittest dump_authentication                          --minPort `expr $PORT01 + 240` --maxPort `expr $PORT01 + 279` $OPTS 2>&1
    scripts/unittest endpoints                                    --minPort `expr $PORT01 + 280` --maxPort `expr $PORT01 + 319` $OPTS 2>&1
    scripts/unittest http_server                                  --minPort `expr $PORT01 + 320` --maxPort `expr $PORT01 + 359` $OPTS 2>&1
    scripts/unittest server_http                                  --minPort `expr $PORT01 + 360` --maxPort `expr $PORT01 + 399` $OPTS 2>&1
    scripts/unittest shell_client                                 --minPort `expr $PORT01 + 400` --maxPort `expr $PORT01 + 439` $OPTS 2>&1
    scripts/unittest shell_server                                 --minPort `expr $PORT01 + 440` --maxPort `expr $PORT01 + 479` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/0  --minPort `expr $PORT01 + 480` --maxPort `expr $PORT01 + 519` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/1  --minPort `expr $PORT01 + 520` --maxPort `expr $PORT01 + 559` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/2  --minPort `expr $PORT01 + 560` --maxPort `expr $PORT01 + 599` $OPTS 2>&1
    scripts/unittest shell_server_aql          --testBuckets 4/3  --minPort `expr $PORT01 + 600` --maxPort `expr $PORT01 + 639` $OPTS 2>&1
    scripts/unittest ssl_server                                   --minPort `expr $PORT01 + 640` --maxPort `expr $PORT01 + 679` $OPTS 2>&1
    scripts/unittest upgrade                                      --minPort `expr $PORT01 + 680` --maxPort `expr $PORT01 + 719` $OPTS 2>&1
    $ENTERPRISE_TESTS
    " | parallel --verbose --header 1 --results log-output --files --no-notice --load 10 --jobs $concurrency > log-output/tests.log
fi

. ./Installation/Pipeline/include/test_check_result.inc $?
