#!/usr/bin/env bash

source default-build-config

set -eux

CUR_DIR=`pwd`
MAKE_DIR=${CUR_DIR}/../
BUILD_DIR=${CUR_DIR}/build

test -e ${BUILD_DIR} || mkdir ${BUILD_DIR}
cd ${BUILD_DIR} && test -e ${HTTPD_TAR} || wget http://ftp.jaist.ac.jp/pub/apache//httpd/${HTTPD_TAR}
cd ${BUILD_DIR} && tar xf ${HTTPD_TAR}
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && test -e ${APR_TAR} || wget http://ftp.jaist.ac.jp/pub/apache//apr/${APR_TAR}
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && test -e ${APR_UTIL_TAR} || wget http://ftp.jaist.ac.jp/pub/apache//apr/${APR_UTIL_TAR}
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && tar xf ${APR_TAR}
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && tar xf ${APR_UTIL_TAR}
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && ln -sf ${APR} apr
cd ${BUILD_DIR}/${HTTPD_VERSION}/srclib && ln -sf ${APR_UTIL} apr-util
cd ${BUILD_DIR}/${HTTPD_VERSION} && test -e apache/bin/httpd || ./configure --prefix=`pwd`/apache --with-included-apr ${HTTPD_CONFIG_OPT}
cd ${BUILD_DIR}/${HTTPD_VERSION} && test -e apache/bin/httpd || make -j10
cd ${BUILD_DIR}/${HTTPD_VERSION} && test -e apache/bin/httpd || make install

cd ${MAKE_DIR}
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs
cd ${CUR_DIR}

cp misc/sleep.cgi `build/${HTTPD_VERSION}/apache/bin/apxs -q exp_cgidir`/

grep -q "VirtualHost 127.0.0.1:8080" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf || sed -i "s/^Listen/#Listen/" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf
grep -q "VirtualHost 127.0.0.1:8080" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf || sed -i "s|__VHOST_DOCROOT__|`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q htdocsdir`|" ${VHOST_CONF}
grep -q "VirtualHost 127.0.0.1:8080" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf || echo "" >> `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

cd ${MAKE_DIR}
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl install
cd ${CUR_DIR}

grep -q "VirtualHost 127.0.0.1:8080" `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf || cat ${VHOST_CONF} >> `${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

cd ${MAKE_DIR}
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl install
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl stop
sleep 1
make APXS=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=${BUILD_DIR}/${HTTPD_VERSION}/apache/bin/apachectl start
sleep 2
cd ${CUR_DIR}
