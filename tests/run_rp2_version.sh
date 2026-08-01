#!/bin/bash

set -o errexit

. ./fetch_roms.sh

make rp2_test_runner

./rp2_test_runner
