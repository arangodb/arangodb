#!/bin/bash

curl -XDELETE "http://localhost:8530/_admin/shutdown"
sleep 2
killall arangod
sleep 2
killall -9 arangod
killall arangosh

