#!/usr/bin/env bash

set -eux

./build/ab-mruby/ab-mruby -m test-case/basic/check_serialized.rb -M test-case/basic/test_serialized.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi
./build/ab-mruby/ab-mruby -m test-case/basic/check_concurrent.rb -M test-case/basic/test_concurrent.rb http://127.0.0.1:8080/cgi-bin/sleep.cgi

killall httpd && sleep 1
