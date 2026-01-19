#!/bin/bash

set -o errexit

. ./fetch_roms.sh

if [[ ! -d rp2040js ]]; then
  git clone --depth 1 https://github.com/c1570/rp2040js.git
  cd rp2040js/
  npm install
  npx tsc demo/bootrom.ts --skipLibCheck
  cd ..
fi
npm install

make

node rp2_test_runner.js
