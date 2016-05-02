#!/bin/bash
# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# Script to be used on GCE slave startup to initiate a load test. Each VM will
# fire an equivalent number of bots and clients. Fine tune the value depending
# on what kind of load test is desired.
#
# Please see https://developers.google.com/compute/docs/howtos/startupscript for
# more details on how to use this script.
#
# The script may run as root, which is counter intuitive. We don't mind much
# because is deleted right after the load test, but still keep this in mind!

set -e

## Settings

# Server to load test against. Please set to your server.
SERVER=https://CHANGE-ME-TO-PROPER-VALUE.appspot.com

# Source code to use.
REPO=https://code.google.com/p/swarming.client.git

# Once the tasks are completed, one can harvest the logs back:
#   scp "slave:/var/log/swarming/*.*" .
LOG=/var/log/swarming

# Delay to wait before starting each client load test. This soften the curve.
CLIENT_DELAY=60s


## Actual work starts here.
# Installs python and git. Do not bail out if it fails, since we do want the
# script to be usable as non-root.
apt-get install -y git-core python || true

# It will end up in /client.
rm -rf swarming_client
git clone $REPO swarming_client
TOOLS_DIR=./swarming_client/tools
mkdir -p $LOG

# This is assuming 8 cores system, so it's worth having 8 different python
# processes, 4 fake bots load processes, 4 fake clients load processes. Each
# load test process creates 250 instances. This gives us a nice 1k bots, 1k
# clients per VM which makes it simple to size the load test, want 20k bots and
# 20k clients? Fire 20 VMs. It assumes a high network throughput per host since
# 250 bots + 250 clients generates a fair amount of HTTPS requests. This is not
# a problem on GCE, these VMs have pretty high network I/O. This may not hold
# true on other Cloud Hosting provider. Tune accordingly!

echo "1. Starting bots."
BOTS_PID=
# Each load test bot process creates multiple (default is 250) fake bots.
for i in {1..4}; do
  $TOOLS_DIR/swarming_load_test_bot.py -S $SERVER --suffix $i \
      --slaves 250 \
      --dump=$LOG/bot$i.json > $LOG/bot$i.log 2>&1 &
  BOT_PID=$!
  echo "  Bot $i pid: $BOT_PID"
  BOTS_PID="$BOTS_PID $BOT_PID"
done

echo "2. Starting clients."
# Each client will send by default 16 tasks/sec * 60 sec, so 16*60*4 = 3840
# tasks per VM.
CLIENTS_PID=
for i in {1..4}; do
  echo "  Sleeping for $CLIENT_DELAY before starting client #$i"
  sleep $CLIENT_DELAY
  $TOOLS_DIR/swarming_load_test_client.py -S $SERVER \
      --send-rate 16 \
      --duration 60 \
      --timeout 180 \
      --concurrent 250 \
      --dump=$LOG/client$i.json > $LOG/client$i.log 2>&1 &
  CLIENT_PID=$!
  echo "  Client $i pid: $CLIENT_PID"
  CLIENTS_PID="$CLIENTS_PID $CLIENT_PID"
done

echo "3. Waiting for the clients to complete; $CLIENTS_PID"
for i in $CLIENTS_PID; do
  echo "  Waiting for $i"
  wait $i
done

echo "4. Sending a Ctrl-C to each bot process so they stop; $BOTS_PID"
for i in $BOTS_PID; do
  kill $i
done

echo "5. Waiting for the bot processes to stop."
for i in $BOTS_PID; do
  echo "  Waiting for $i"
  wait $i || true
done

echo "6. Load test is complete."
touch $LOG/done
