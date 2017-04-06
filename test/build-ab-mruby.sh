#!/usr/bin/env bash

source default-build-config

set -eux

CUR_DIR=`pwd`
MAKE_DIR=${CUR_DIR}/../
BUILD_DIR=${CUR_DIR}/build

test -e ${BUILD_DIR} || mkdir ${BUILD_DIR}
cd ${BUILD_DIR} && test -e ab-mruby || git clone --recursive https://github.com/matsumoto-r/ab-mruby.git
cd ${BUILD_DIR}/ab-mruby && make
