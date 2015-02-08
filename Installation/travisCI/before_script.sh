#!/bin/bash
set -e

d='UnitTests/HttpInterface'

echo
echo "$0: switching into ${d}"
cd "${d}"

echo
echo "$0: installing bundler"
gem install bundler

echo
echo "$0: executing bundle"
bundle
