##
##  Makefile -- Build procedure for sample mod_vhost_maxclients Apache module
##	  MATSUMOTO, Ryosuke
##

# target module source
TARGET=mod_vhost_maxclients.c

#   the used tools
APXS=apxs
APACHECTL=apachectl

#   additional user defines, includes and libraries
DEF=
INC=
LIB=
WC=-Wc,-std=c99

#   the default target
all: mod_vhost_maxclients.so

#   compile the DSO file
mod_vhost_maxclients.so: $(TARGET)
	$(APXS) -c $(DEF) $(INC) $(LIB) $(WC) $(TARGET)

#   install the DSO file into the Apache installation
#   and activate it in the Apache configuration
install: all
	$(APXS) -i -a -n 'vhost_maxclients' .libs/mod_vhost_maxclients.so

#   cleanup
clean:
	-rm -rf .libs *.o *.so *.lo *.la *.slo *.loT

#   clobber
clobber: clean
	-rm -rf test/build

#   the general Apache start/restart/stop procedures
start:
	$(APACHECTL) -k start
restart:
	$(APACHECTL) -k restart
stop:
	$(APACHECTL) -k stop

#
# for apache2.4.x with worker env example
#

HTTPD_VERSION=httpd-2.4.25
HTTPD_CONFIG_OPT="--with-mpm=worker"
APR=apr-1.5.2
APR_UTIL=apr-util-1.5.4
HTTPD_TAR=$(HTTPD_VERSION).tar.gz
APR_TAR=$(APR).tar.gz
APR_UTIL_TAR=$(APR_UTIL).tar.gz
APXS_CHECK_CMD="./$(HTTPD_VERSION)/apache/bin/apachectl -v"
VHOST_CONF="conf/mod_vhost_maxclients.conf.2.4"

build_apache:
	cd test && \
	HTTPD_VERSION=$(HTTPD_VERSION) \
	HTTPD_CONFIG_OPT=$(HTTPD_CONFIG_OPT) \
	APR=$(APR) \
	APR_UTIL=$(APR_UTIL) \
	HTTPD_TAR=$(HTTPD_TAR) \
	APR_TAR=$(APR_TAR) \
	APR_UTIL_TAR=$(APR_UTIL_TAR) \
	APXS_CHECK_CMD=$(APXS_CHECK_CMD) \
	VHOST_CONF=$(VHOST_CONF) \
	./build-apache.sh

build_ab_mruby:
	cd test && \
	HTTPD_VERSION=$(HTTPD_VERSION) \
	HTTPD_CONFIG_OPT=$(HTTPD_CONFIG_OPT) \
	APR=$(APR) \
	APR_UTIL=$(APR_UTIL) \
	HTTPD_TAR=$(HTTPD_TAR) \
	APR_TAR=$(APR_TAR) \
	APR_UTIL_TAR=$(APR_UTIL_TAR) \
	APXS_CHECK_CMD=$(APXS_CHECK_CMD) \
	VHOST_CONF=$(VHOST_CONF) \
	./build-ab-mruby.sh

# run all test
test: build_apache build_ab_mruby
	cd test && \
	HTTPD_VERSION=$(HTTPD_VERSION) \
	HTTPD_CONFIG_OPT=$(HTTPD_CONFIG_OPT) \
	APR=$(APR) \
	APR_UTIL=$(APR_UTIL) \
	HTTPD_TAR=$(HTTPD_TAR) \
	APR_TAR=$(APR_TAR) \
	APR_UTIL_TAR=$(APR_UTIL_TAR) \
	APXS_CHECK_CMD=$(APXS_CHECK_CMD) \
	VHOST_CONF=$(VHOST_CONF) \
	./all-test-run.sh

.PHONY: test build
