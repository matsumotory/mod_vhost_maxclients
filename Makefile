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

build_apache:
	cd test && ./build-apache.sh

build_ab_mruby:
	cd test && ./build-ab-mruby.sh

# run all test
test: build_apache build_ab_mruby
	cd test && ./all-test-run.sh

.PHONY: test build
