#!/usr/bin/env bash

set -eux

CUR_DIR=`pwd`
MAKE_DIR=${CUR_DIR}/../
BUILD_DIR=${CUR_DIR}/build

${BUILD_DIR}/ab-mruby/ab-mruby -m test-case/timeslot/check.rb -M test-case/timeslot/test.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi

killall httpd && sleep 1

# restore default configuration
mv `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig \
  `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

