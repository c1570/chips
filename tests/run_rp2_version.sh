#!/bin/bash

set -o errexit

. ./fetch_roms.sh

if [[ ! -d rp2350js ]]; then
  git clone --depth 1 https://github.com/c1570/rp2350js.git rp2350js
  cd rp2350js/
  npm install
  npm run build
  cd ..
fi
npm install

make

node rp2_test_runner.js
