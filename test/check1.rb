# Usage: ./ab-mruby -m bench.rb [http[s]://]hostname[:port]/path

target_hosts = ["127.0.0.1"]
target_paths = ["/cgi-bin/sleep.cgi"]

unless target_paths.include? get_config("TargetPath")
  puts "invalid path #{get_config("TargetPath")}"
  exit
end

unless target_hosts.include? get_config("TargetHost")
  puts "invalid host #{get_config("TargetHost")}"
  exit
end

add_config(
  "TotalRequests"         => 10,                       # int
  "Concurrency"           => 1,                        # int max 20000
  "KeepAlive"             => true,                      # true or false or nil
  "ShowProgress"          => false,                      # true, false or nil
  "ShowPercentile"        => false,                      # true, false or nil
  "ShowConfidence"        => false,                      # true, false or nil
  "VerboseLevel"          => 1,                         # int 1 ~ 5
  "AddHeader"             => 'Host: test001.example.local',
)

