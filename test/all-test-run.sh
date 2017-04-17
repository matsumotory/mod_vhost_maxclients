#!/bin/bash

source default-build-config

set -eux

# basi test
./test-case/basic.sh

# DRYRUN test
./setup/dryrun.sh
./test-case/dryrun.sh

# Timeslot test
./setup/timeslot.sh
./test-case/timeslot.sh

# RequestRegexp test
./setup/regexp.sh
./test-case/regexp.sh
