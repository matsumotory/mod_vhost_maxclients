##
##  Makefile -- Build procedure for sample mod_vhost_maxclients Apache module
##	  MATSUMOTO, Ryosuke
##

# target module source
TARGET=mod_vhost_maxclients.c

#   the used tools
APXS=apxs
APACHECTL=apachectl -k

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

#   the general Apache start/restart/stop procedures
start:
	$(APACHECTL) start
restart:
	$(APACHECTL) restart
stop:
	$(APACHECTL) stop

test:
	git clone --recursive https://github.com/matsumoto-r/ab-mruby.git
	cd ab-mruby && make
	cd ab-mruby && ./ab-mruby -m ../test/check1.rb -M ../test/test1.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
	cd ab-mruby && ./ab-mruby -m ../test/check.rb -M ../test/test.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi

test2:
	cd ab-mruby && ./ab-mruby -m ../test/check.rb -M ../test/test1.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi


.PHONY: test
