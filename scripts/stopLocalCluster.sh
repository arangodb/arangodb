#!/bin/bash
killall arangod
sleep 5
killall -9 arangod
killall -9 etcd-arango
killall arangosh

