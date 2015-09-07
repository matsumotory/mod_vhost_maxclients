# print ab-mruby headers
print <<EOS
======================================================================
This is ab-mruby using ApacheBench Version 2.3 <$Revision: 1430300 $>
Licensed to MATSUMOTO Ryosuke, https://github.com/matsumoto-r/ab-mruby

                            TEST PHASE
           (concurrent connection over vhot maxclients)

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
  "Non2xxResponses".should_be_over           5
end

test_run
