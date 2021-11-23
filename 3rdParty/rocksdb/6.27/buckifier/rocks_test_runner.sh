#!/usr/bin/env bash
# Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved.
# Create a tmp directory for the test to use
TEST_DIR=$(mktemp -d /dev/shm/fbcode_rocksdb_XXXXXXX)
# shellcheck disable=SC2068
TEST_TMPDIR="$TEST_DIR" $@ && rm -rf "$TEST_DIR"
