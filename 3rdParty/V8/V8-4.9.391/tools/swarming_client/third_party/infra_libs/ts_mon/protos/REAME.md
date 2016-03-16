Updating the *.proto files: see go/updating-tsmon-protos

To generate the `*_pb2.py` files from the `*.proto` files:

    cd infra_libs/ts_mon/protos
    protoc --python_out=. *.proto

protoc version tested: 2.6.0
