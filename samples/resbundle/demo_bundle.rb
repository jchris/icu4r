require 'icu4r'
v = UResourceBundle.open(File.expand_path("appmsg"), "ru")
puts v["icu4r_hello"]
puts v["icu4r_classes"]
