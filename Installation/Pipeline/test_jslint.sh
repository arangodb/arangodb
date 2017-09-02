#!/bin/bash

type="test_jslint"

echo "`date +%T` jslinting..."
./utils/jslint.sh
result=$?

echo "`date +%T` done..."

exit $result
