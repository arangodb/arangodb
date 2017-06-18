#!/bin/bash

type="test_jslint"

. ./Installation/Pipeline/include/test_setup_tmp.inc

echo "`date +%T` jslinting..."
./utils/jslint.sh

echo "`date +%T` done..."
