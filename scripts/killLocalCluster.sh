#!/usr/bin/env bash

killall arangod
sleep 2
killall -9 arangod
killall arangosh

