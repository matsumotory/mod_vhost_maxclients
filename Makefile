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
	-rm -rf build

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
HTTPD_VERSION=httpd-2.4.23
HTTPD_CONFIG_OPT="--with-mpm=worker"
APR=apr-1.5.2
APR_UTIL=apr-util-1.5.4
HTTPD_TAR=$(HTTPD_VERSION).tar.gz
APR_TAR=$(APR).tar.gz
APR_UTIL_TAR=$(APR_UTIL).tar.gz
APXS_CHECK_CMD="./$(HTTPD_VERSION)/apache/bin/apachectl -v"
VHOST_CONF="test/mod_vhost_maxclients.conf.2.4"

build:
	test -e build || mkdir build
	cd build && test -e $(HTTPD_TAR) || wget http://ftp.jaist.ac.jp/pub/apache//httpd/$(HTTPD_TAR)
	cd build && tar xf $(HTTPD_TAR)
	cd build/$(HTTPD_VERSION)/srclib && test -e $(APR_TAR) || wget http://ftp.jaist.ac.jp/pub/apache//apr/$(APR_TAR)
	cd build/$(HTTPD_VERSION)/srclib && test -e $(APR_UTIL_TAR) || wget http://ftp.jaist.ac.jp/pub/apache//apr/$(APR_UTIL_TAR)
	cd build/$(HTTPD_VERSION)/srclib && tar xf $(APR_TAR)
	cd build/$(HTTPD_VERSION)/srclib && tar xf $(APR_UTIL_TAR)
	cd build/$(HTTPD_VERSION)/srclib && ln -sf $(APR) apr
	cd build/$(HTTPD_VERSION)/srclib && ln -sf $(APR_UTIL) apr-util
	cd build/$(HTTPD_VERSION) && test -e apache/bin/httpd || ./configure --prefix=`pwd`/apache --with-included-apr $(HTTPD_CONFIG_OPT)
	cd build/$(HTTPD_VERSION) && test -e apache/bin/httpd || make -j10
	cd build/$(HTTPD_VERSION) && test -e apache/bin/httpd || make install
	make APXS=build/$(HTTPD_VERSION)/apache/bin/apxs
	cp test/sleep.cgi `build/$(HTTPD_VERSION)/apache/bin/apxs -q exp_cgidir`/
	sed -i "s/^Listen/#Listen/" `build/$(HTTPD_VERSION)/apache/bin/apxs -q sysconfdir`/`build/$(HTTPD_VERSION)/apache/bin/apxs -q progname`.conf
	sed -i "s|__VHOST_DOCROOT__|`build/$(HTTPD_VERSION)/apache/bin/apxs -q htdocsdir`|" $(VHOST_CONF)
	cat $(VHOST_CONF) >> `build/$(HTTPD_VERSION)/apache/bin/apxs -q sysconfdir`/`build/$(HTTPD_VERSION)/apache/bin/apxs -q progname`.conf
	make APXS=build/$(HTTPD_VERSION)/apache/bin/apxs APACHECTL=build/$(HTTPD_VERSION)/apache/bin/apachectl install
	make APXS=build/$(HTTPD_VERSION)/apache/bin/apxs APACHECTL=build/$(HTTPD_VERSION)/apache/bin/apachectl restart

test1: build
	cd build && test -e ab-mruby || git clone --recursive https://github.com/matsumoto-r/ab-mruby.git
	cd build/ab-mruby && make
	cd build/ab-mruby && ./ab-mruby -m ../../test/check1.rb -M ../../test/test1.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
	cd build/ab-mruby && ./ab-mruby -m ../../test/check.rb -M ../../test/test.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
	killall httpd && sleep 1

#
# test for global dryrun
#
fixup_test2_conf: test1
	cp `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig
	echo "VhostMaxClientsDryRun On" >> `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf
	make APXS=build/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=build/${HTTPD_VERSION}/apache/bin/apachectl stop
	sleep 1
	make APXS=build/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=build/${HTTPD_VERSION}/apache/bin/apachectl start
	sleep 1

test2: fixup_test2_conf
	cd build/ab-mruby && ./ab-mruby -m ../../test/check.rb -M ../../test/test1.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
	killall httpd && sleep 1
	mv `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

#
# test for out of range of time slot #8
#
fixup_test3_conf: test2
	cp `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig
	sed -e "s/0000 2358/0000 0000/" `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf | tee `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf
	make APXS=build/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=build/${HTTPD_VERSION}/apache/bin/apachectl stop
	sleep 1
	make APXS=build/${HTTPD_VERSION}/apache/bin/apxs APACHECTL=build/${HTTPD_VERSION}/apache/bin/apachectl start
	sleep 1

test3: fixup_test3_conf
	# complete all requests with 10 concurency for out of range of time slot
	cd build/ab-mruby && ./ab-mruby -m ../../test/check.rb -M ../../test/test1.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
	killall httpd && sleep 1
	mv `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf.orig `build/${HTTPD_VERSION}/apache/bin/apxs -q sysconfdir`/`build/${HTTPD_VERSION}/apache/bin/apxs -q progname`.conf

# run all test chain dependency
test: test3

.PHONY: test build
