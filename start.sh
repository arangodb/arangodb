mkdir agent dbServer-1 dbServer-2 coordinator-1 coordinator-2

./build/bin/arangod --configuration=none --agency.activate=true --agency.size=1 --agency.pool-size=1 --agency.my-address=tcp://192.168.173.238:4000 --agency.endpoint=tcp://192.168.173.238:4000 --server.authentication=false --server.storage-engine=rocksdb --javascript.startup-directory=./js --javascript.app-path=agent/apps --server.endpoint=tcp://192.168.173.238:4000 --log.file=agent/log agent/data &

./build/bin/arangod --configuration=none --cluster.agency-endpoint=tcp://192.168.173.238:4000 --cluster.my-role=PRIMARY --cluster.my-address=tcp://192.168.173.238:4050 --server.authentication=false --server.storage-engine=rocksdb --javascript.startup-directory=./js --javascript.app-path=dbServer-1/apps --server.endpoint=tcp://192.168.173.238:4050 --log.file=dbServer-1/arangod.log dbServer-1/data &

./build/bin/arangod --configuration=none --cluster.agency-endpoint=tcp://192.168.173.238:4000 --cluster.my-role=PRIMARY --cluster.my-address=tcp://192.168.173.238:4100 --server.authentication=false --server.storage-engine=rocksdb --javascript.startup-directory=./js --javascript.app-path=dbServer-2/apps --server.endpoint=tcp://192.168.173.238:4100 --log.file=dbServer-2/arangod.log dbServer-2/data &

./build/bin/arangod --configuration=none --cluster.agency-endpoint=tcp://192.168.173.238:4000 --cluster.my-role=COORDINATOR --cluster.my-address=tcp://192.168.173.238:4150 --server.authentication=false --server.storage-engine=rocksdb --javascript.startup-directory=./js --javascript.app-path=coordinator-1/apps --server.endpoint=tcp://192.168.173.238:4150 --log.file=coordinator-1/arangod.log coordinator-1/data &

./build/bin/arangod --configuration=none --cluster.agency-endpoint=tcp://192.168.173.238:4000 --cluster.my-role=COORDINATOR --cluster.my-address=tcp://192.168.173.238:4200 --server.authentication=false --server.storage-engine=rocksdb --javascript.startup-directory=./js --javascript.app-path=coordinator-2/apps --server.endpoint=tcp://192.168.173.238:4200 --log.file=coordinator-2/arangod.log coordinator-2/data &
