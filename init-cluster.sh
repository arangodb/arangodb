
mkdir -p data-pavel data-perry data-claus

NAME="meier"
ETCD="http://127.0.0.1:4001"
echo "initialising cluster $NAME"
bin/arangom -a "$ETCD" -p "/$NAME/" init
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Plan/DBServers/Pavel" -d "value=none" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/DBServers/Pavel" -d "value=none" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Plan/DBServers/Perry" -d "value=none" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/DBServers/Perry" -d "value=none" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Plan/Coordinators/Claus" -d "value=none" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/Coordinators/Claus" -d "value=none" || exit 1

curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/MapIDToEndpoint/Pavel" -d "value=tcp://127.0.0.1:8530" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/MapIDToEndpoint/Perry" -d "value=tcp://127.0.0.1:8531" || exit 1
curl --silent --dump - -L -X PUT "$ETCD/v2/keys/$NAME/Target/MapIDToEndpoint/Claus" -d "value=tcp://127.0.0.1:8529" || exit 1

echo
echo start arangod with:
echo "Pavel:   bin/arangod --cluster.my-id Pavel --cluster.agency-prefix $NAME --cluster.agency-endpoint tcp://127.0.0.1:4001 --server.endpoint tcp://127.0.0.1:8530 data-pavel"
echo "Perry:   bin/arangod --cluster.my-id Perry --cluster.agency-prefix $NAME --cluster.agency-endpoint tcp://127.0.0.1:4001 --server.endpoint tcp://127.0.0.1:8531 data-perry"
echo "Claus:   bin/arangod --cluster.my-id Claus --cluster.agency-prefix $NAME --cluster.agency-endpoint tcp://127.0.0.1:4001 --server.endpoint tcp://127.0.0.1:8529 data-claus"
echo test with:
echo curl -X GET http://localhost:8529/_admin/sharding-test/_admin/time
