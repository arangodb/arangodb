#!/bin/bash

echo "${@:2} | tee $1"

${@:2} | tee $1
