require 'icu4r'
root = UResourceBundle.open(nil, "en")
today = Time.now
UString::list_locales.each do |locale|
	b = UResourceBundle.open(nil, locale)
	lang, ctry, var = locale.split '_', 3
	ctry = var ? var : ctry
	puts [ 
		locale, 
		"("+root["Countries"][ctry].to_s + " : " +	root["Languages"][lang].to_s+")", 
		"("+b["Countries"][ctry].to_s + " : " +  b["Languages"][lang].to_s+")",
	 	"[{0,date,long}]({1,number,currency})".u.fmt(locale, today, 123.45),
		b["ExemplarCharacters"]
	].join("\t")

end
