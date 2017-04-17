#!/usr/bin/env bash

CUR_DIR=`pwd`
MAKE_DIR=${CUR_DIR}/../
BUILD_DIR=${CUR_DIR}/build

cp `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf \
  `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig

grep -q "IgnoreVhostMaxClientsRequestRegexp" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf || \
  echo "IgnoreVhostMaxClientsRequestRegexp .cgi" >> `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

cd ${MAKE_DIR}
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl stop
sleep 1
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl start
sleep 2

