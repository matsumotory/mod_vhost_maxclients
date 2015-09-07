module Kernel
  def test_suite &blk
    @@r = get_config
    @@t = blk
  end
  def should_be val
    res = @@r[self] == val
    puts "[TEST CASE] #{self} (#{@@r[self]}) should be #{val}: #{res}"
    test_fail res
  end
  def should_be_over val
    res = @@r[self] > val
    puts "[TEST CASE] #{self} (#{@@r[self]}) should be over #{val}: #{res}"
    test_fail res
  end
  def should_be_under val
    res = @@r[self] < val
    puts "[TEST CASE] #{self} (#{@@r[self]}) should be under #{val}: #{res}"
    test_fail res
  end
  def test_fail res
    raise "~~~ test failed ~~~" if res == false
  end
  def test_run
    @@t.call
    puts "=== all test passed ==="
    true
  end
end

# print ab-mruby headers
print <<EOS
======================================================================
This is ab-mruby using ApacheBench Version 2.3 <$Revision: 1430300 $>
Licensed to MATSUMOTO Ryosuke, https://github.com/matsumoto-r/ab-mruby

                            TEST PHASE
           (concurrent connection under vhot maxclients)

======================================================================
EOS

# define test suite
test_suite do
  "TargetServerHost".should_be               "127.0.0.1"
  "TargetServerPort".should_be               8080
  "TargetDocumentPath".should_be             "/cgi-bin/sleep.cgi"
  "WriteErrors".should_be                    0
  "CompleteRequests".should_be               10
  "ConnetcErrors".should_be                  0
  "ReceiveErrors".should_be                  0
  "ExceptionsErrors".should_be               0
  "Non2xxResponses".should_be                0
  "LengthErrors".should_be                   0
  "FailedRequests".should_be                 0
end

test_run
