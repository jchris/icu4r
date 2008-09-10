require './icu4r'
require 'test/unit'
class UnicodeStringTest < Test::Unit::TestCase

  def test_string
    a = u("абвг", "utf8")
    b = u("абвг", "utf8")
    assert_equal(a,b )
  end
  
  def test_casecmp
    assert_equal(0, u("Сцуко").casecmp("сЦуКо".u))
    assert_equal(-1, u("Сцук").casecmp("сЦуКо".u))
    assert_equal(1, u("Сцуко").casecmp("сЦуК".u))
  end
  
  def test_match
    assert_match(ure("абвг"), u("абвг"))
    assert("аавг".u !~ ure("^$"))
    assert("авб\n".u !~ ure("^$"))
    assert("абв".u !~ ure("^г*$"))
    assert_equal("".u, ("абв".u =~ ure("г*$"))[0])
    assert("".u =~ ure("^$"))
    assert("абвабв".u =~ ure( ".*а")  )
    assert("абвабв".u =~ ure( ".*в")  )
    assert("абвабв".u =~ ure( ".*?а") )
    assert("абвабв".u =~ ure( ".*?в") )
    assert(ure("(.|\n)*?\n(б|\n)") =~ u("а\nб\n\n"))
 end
 
 def test_sub
    x = "a.gif".u
    assert_equal("gif".u, x.sub(ure(".*\\.([^\\.]+)$"), "$1".u))
    assert_equal("b.gif".u, x.sub(ure(".*\\.([^\\.]+)$"), "b.$1".u))
    assert_equal(x, "a.gif".u)
    x.sub!(/gif/.U, ''.u)
    assert_equal(x, "a.".u)
    x= "1234561234".u
    x.sub!(/123/.U, "".u)
    assert_equal(x,  "4561234".u)

 end

 
 def test_case_fold
    assert_equal("А".u, "а".u.upcase!)
    assert_equal("а".u, ("А".u.downcase!))

    s = "аБв".u
    s.upcase
    assert_equal("аБв".u, s)
    s.upcase!
    assert_equal("АБВ".u, s)
    
    s = "аБв".u
    s.downcase
    assert_equal("аБв".u, s)
    s.downcase!
    assert_equal("абв".u, s)
 end

 def test_index
   assert_equal( "hello".u.rindex('e'.u), 1)            
   assert_equal( "hello".u.rindex('l'.u) , 3)          
   assert_equal( "hello".u.rindex('a'.u), nil)
   assert_equal( "hello".u.index('e'.u),1)              
   assert_equal( "hello".u.index('lo'.u),3)             
   assert_equal( "hello".u.index('a'.u), nil)           
   assert_equal( "hello".u.index(ure('[aeiou]'), -3),  4)
   assert_equal( "hello".u.rindex(ure('[aeiou]'), -2), 1)
    
    assert_equal(1, S("hello").index(S("ell")))
    assert_equal(2, S("hello").index(/ll./.U))

    assert_equal(3, S("hello").index(S("l"), 3))
    assert_equal(3, S("hello").index(/l./.U, 3))

    assert_nil(S("hello").index(S("z"), 3))
    assert_nil(S("hello").index(/z./.U, 3))

    assert_nil(S("hello").index(S("z")))
    assert_nil(S("hello").index(/z./.U))

 end
 
 def test_insert
     assert_equal("abcd".u.insert(0, 'X'.u)    , "Xabcd".u)
     assert_equal("abcd".u.insert(3, 'X'.u)    , "abcXd".u)
     assert_equal("abcd".u.insert(4, 'X'.u)    , "abcdX".u)
     assert_equal("abcd".u.insert(-3, 'X'.u)   , "abXcd".u)
     assert_equal("abcd".u.insert(-1, 'X'.u)   , "abcdX".u)  
 end

 def test_include
    assert( "hello".u.include?("lo".u))
    assert(!("hello".u.include?("ol".u)))
 end

 def test_init
   assert_equal( "нах!".u, UString.new("нах!".u))
   a = "ГНУ!".u
   a.replace("ФИГНУ!".u)
   assert_equal(a, "ФИГНУ!".u)
   assert_equal(a, a.clone)
 end
 
 def test_aref
    a = "hello there".u
    assert_equal('e'.u, a[1])                   #=> 'e'
    assert_equal('ell'.u, a[1,3])               #=> "ell"
    assert_equal('ell'.u, a[1..3])                #=> "ell"
    assert_equal('er'.u, a[-3,2])                #=> "er"
    assert_equal('her'.u, a[-4..-2])              #=> "her"
    assert_nil(a[12..-1])              #=> nil
    assert_equal(''.u, a[-2..-4])      #=> ""
    assert_equal('ell'.u, a[ure('[aeiou](.)\1')])      #=> "ell"
    assert_equal('ell'.u, a[ure('[aeiou](.)\1'), 0])   #=> "ell"
    assert_equal('l'.u,   a[ure('[aeiou](l)\1'), 1])   #=> "l"
    assert_nil( a[ure('[aeiou](.)$1'), 2])   #=> nil
    assert_equal('lo'.u, a["lo".u])                #=> "lo"  
    assert_nil(a["bye".u])               #=> nil   
  end

  def test_slice_bang
    string = "this is a string".u
    assert_equal(string.slice!(2) , 'i'.u)
    assert_equal(string.slice!(3..6) , " is ".u) 
    assert_equal(string.slice!(ure("s.*t")) , "sa st".u)
    assert_equal(string.slice!("r".u) , "r".u)
    assert_equal(string , "thing".u)
    a = "test".u
    a[0] = "BEA".u
    assert_equal("BEAest".u, a)
  end

  def test_gsub
    assert_equal("hello".u.gsub(ure("[aeiou]"), '*'.u)              , "h*ll*".u)
    assert_equal("hello".u.gsub(ure("([aeiou])"), '<$1>'.u)         , "h<e>ll<o>".u)
    i = 0 
    assert_equal("12345".u , "hello".u.gsub(ure(".")) {|s| i+=1; i.to_s})
    assert_equal("214365".u, "123456".u.gsub(ure("(.)(.)")) {|s| s[2] + s[1] })
    a = "test".u
    a.gsub!(/t/.U, a)
    assert_equal("testestest".u, a)
  end

  def test_ure_case_eq
    a = "HELLO".u
    v = case a
      when ure("^[a-z]*$"); "Lower case"
      when ure("^[A-Z]*$"); "Upper case"
      else;           "Mixed case"
    end
    assert_equal('Upper case', v)
  end
  
 #  UString::strcoll("ÆSS".u, "AEß".u, "de", 0)
 def test_empty
    assert(! "hello".u.empty?)
    assert("".empty?)
    assert("test".u.clear.empty?)
    assert(" \t\n".u.strip.empty?)
 end

 def test_clear
   a = "test".u
   a.clear
   assert_equal(0, a.length)
 end
 
 def test_length
    assert_equal(10, "12345АБВГД".u.length)
    assert_equal(0,"".u.length)
    assert_equal(3,"abc".u.length)
 end
 
 def test_replace
    s = "hello".u
    s.replace("world".u)
    assert_equal(s, "world".u)
 end
 
 def test_cmp	
    assert_equal("абвгде".u <=> "абвгд".u     , 1  )
    assert_equal("абвгде".u <=> "абвгде".u    , 0  )        	
    assert_equal("абвгде".u <=> "абвгдеж".u   , -1 )        	
    assert_equal("абвгде".u <=> "АБВГДЕ".u    , -1  ) # UCA
 end

 def test_plus
   assert_equal("сложение".u, "сло".u + "жение".u)
 end

 def test_times
   assert_equal("ААААА".u, "А".u * 5)
 end

 def test_concat
   assert_equal("сложение".u, "сло".u << "жение".u)
   assert_equal("сложение".u, "сло".u.concat("жение".u))
   a = "сло".u
   a << "жение".u
   assert_equal("сложение".u, a)
 end
 
 def test_search
      a = "A quick brown fox jumped over the lazy fox dancing foxtrote".u                                          	
      assert_equal(a.search("fox".u) , [14..16, 39..41, 51..53])
      assert_equal(a.search("FoX".u) , [])
      assert_equal(a.search("FoX".u, :ignore_case => true) , [14..16, 39..41, 51..53])
      assert_equal(a.search("FoX".u, :ignore_case => true, :whole_words => true) , [14..16, 39..41])
      assert_equal(a.search("FoX".u, :ignore_case => true, :whole_words => true, :limit => 1) , [14..16])
      
      b = "Iñtërnâtiônàlizætiøn îs cọmpłèx".u.upcase 
      assert_equal(b, "IÑTËRNÂTIÔNÀLIZÆTIØN ÎS CỌMPŁÈX".u)
      assert_equal(b.search("nâtiôn".u, :locale => "en") , [])
      assert_equal(b.search("nation".u) , [])
      assert_equal(b.search("nation".u, :locale => "en", :ignore_case_accents => true) , [5..10])
      assert_equal(b.search("nâtiôn".u, :locale => "en", :ignore_case => true) , [5..10])
      assert_equal(b.search("zaeti".u, :locale => "en" ) , [])
      assert_equal(b.search("zaeti".u, :locale => "en", :ignore_case => true) , [])
      assert_equal(b.search("zaeti".u, :locale => "en", :ignore_case_accents => true) , [14..17])
      assert_equal("İSTANBUL".u.search("istanbul".u, :locale => 'tr', :ignore_case => true), [0..7])
      assert_equal("ёжий".u.norm_D.search("ЕЖИЙ".u, :locale => 'ru', :canonical => true, :ignore_case_accents => true), [0..4])
  end

  def test_dollar_sign_regexp
      assert_equal("te$et".u, "test".u.gsub(/s/.U, '$e'.u))
  end
  
  def test_codepoints
      a=[0x01234, 0x0434, 0x1D7D9, ?t, ?e, ?s]
      b=a.pack("U*").u
      assert_equal(a, b.codepoints)
      assert_equal(b, a.to_u)
  end

    def test_chars
    chr =      ["I", "Ñ", "T", "Ë", "R", "N", "Â", "T", "I", "Ô", "N", "À", "L", "I", "Z", "Æ", "T", "I", "Ø", "N" ]
    chr = chr.collect {|s| s.to_u.norm_C}
    assert_equal(chr, "Iñtërnâtiônàlizætiøn".u.upcase.norm_D.chars)

    end


    def test_fmt
      assert_equal("b a".u, "{1} {0}".u.fmt("en", "a".u, "b".u))
      assert_equal("12,345.56".u, "{0, number}".u.fmt("en", 12345.56))
      assert_equal("$12,345.56".u, "{0, number, currency}".u.fmt("en_US", 12345.56))
      assert_equal("20:15:01 13/01/2006".u, "{0,date,HH:mm:ss dd/MM/yyyy}".u.fmt("en", Time.local(2006,"jan",13,20,15,1)))
    end

    def test_norm
	  v="Iñtërnâtiônàlizætiøn".u
	  assert_equal("Iñtërnâtiônàlizætiøn".u,  v.norm_C)
	  assert_equal("Iñtërnâtiônàlizætiøn".u, 	v.norm_D)
	  assert_equal("Iñtërnâtiônàlizætiøn".u, 	    v.norm_D.norm_FCD)
	  assert_equal("Iñtërnâtiônàlizætiøn".u,v.norm_D.norm_KC)
    end

    def test_scan
      a = "cruel world".u
      assert_equal(a.scan(/\w+/.U)        ,["cruel".u , "world".u ])
      assert_equal(a.scan(/.../.U)        ,["cru".u , "el ".u , "wor".u ])
      assert_equal(a.scan(/(...)/.U)      ,["cru".u , "el ".u , "wor".u ])
      assert_equal(a.scan(/(..)(..)/.U)   ,[["cr".u , "ue".u ], ["l ".u , "wo".u ]]	    )
    end
    def S(str)
      str.to_u
    end
    def test_split
		  re = URegexp.new("[,:/]".u)
		  assert_equal(["split test".u ,  "west".u ,  "best".u ,  "east".u ], re.split("split test,west:best/east".u, nil))
      assert_equal(["split test".u, "west:best/east".u], re.split("split test,west:best/east".u, 2))
      assert_equal([S("a"), S("b"), S("c")], S("a   b\t c").split(S("\\s+")))
      assert_equal([S(" a "), S(" b "), S(" c ")], S(" a | b | c ").split(S("\\|")))
      assert_equal([S("a"), S("b"), S("c")], S("aXXbXXcXX").split(/X./.U))
      assert_equal([S("a|b|c")], S("a|b|c").split(S('\|'), 1))
      assert_equal([S("a"), S("b|c")], S("a|b|c").split(S('\|'), 2))
      assert_equal([S("a"), S("b"), S("c")], S("a|b|c").split(S('\|'), 3))
      assert_equal([S("a"), S("b"), S("c")], S("a|b|c|").split(S('\|'), -1))
      assert_equal([S("a"), S("b"), S("c"), S("") ], S("a|b|c||").split(S('\|'), -1))
      assert_equal([S("a"), S(""), S("b"), S("c")], S("a||b|c|").split(S('\|'), -1))
    end


    def test_strcoll
    assert_equal(0,  UString::strcoll("a".u, "a".u))
    assert_equal(-1, UString::strcoll("y".u, "k".u, "lv"))
    assert_equal(1,  UString::strcoll("я".u, "а".u))
    assert_equal(1,  UString::strcoll("я".u, "А".u, "ru"))
    assert_equal(0,  UString::strcoll("İSTANBUL".u, "istanbul".u, "tr", 0))
    assert_equal(0,  UString::strcoll("ой её".u, "ОЙ ЕЁ".u, "ru", 1))
    end

    def test_gsub_block
    	a = "АБРАКАДАБРА".u
	r = URegexp.new("(.)(.)(А)".u, URegexp::IGNORECASE)
	b = a.gsub(r) do |m|
	  assert_equal("ава".u, "бравада".u.gsub(r) {|v| v[3]} )
	  m[3] + m[2] + m[1]
	end
	assert_equal("ААРБКАДААРБ".u, b)
    end

    def test_match_range
    	t = "test\ntext".u
	m = (t =~ /^.+$/m.U)
	assert_equal('test'.u, m[0])
	assert_equal(0..3, m.range(0))
    end

    def test_resbundle
    	b = UResourceBundle.open(nil, "en")
	assert_equal("Russia".u, b["Countries"]["RU"])
	b = UResourceBundle.open(nil, "ru")
	assert_equal("Россия".u, b["Countries"]["RU"])

    end

    def test_translit
    	assert_equal('zees ees A  tfs t'.u, "This is A test".u.translit("null".u, "a>b;b>c;c>d;d>e;e>f;i>ee;[Tt]h>z;t>\\ t".u))
	assert_equal("matsumoto yukihiro".u.translit("Latin-Hiragana".u), "まつもと ゆきひろ".u)
    end

    def test_parse_double
         assert_equal(456, "456".u.to_f)
    	 assert_equal("123,001".u.to_f("ru"),  123.001)
	 assert_equal("123,001".u.to_f("en"), 123001.0)
	 assert_equal("Got 123,001".u.to_f("en", "Got ###,###".u), 123001)
	 assert_equal(123.45, "١٢٣٫٤٥".u.to_f("ar_YE"))
    end

    def test_unescape
	    a = '\u0054\u0068\u0069\u0073\u0020\u0069\u0073\u0020\u0041\u0020\u0074\u0065\u0073\u0074\n!'
	    assert_equal("This is A test\n!", a.u.unescape.to_s)
    end

    def test_ranges
	    v = "\\x{1D7D9}\\x{1d7da}\\x{1d7db}!".u.unescape
	    assert_equal(7, v.length)
	    assert_equal(4, v.point_count)
	    assert_equal(0..0, v.conv_unit_range(0..1))
	    assert_equal(0..1, v.conv_unit_range(0..2))
	    assert_equal(0..3, v.conv_unit_range(0..-1))
	    assert_equal(2..3, v.conv_unit_range(-3..-1))
	    
	    assert_equal(0..3, v.conv_point_range(0..1))
	    assert_equal(0..5, v.conv_point_range(0..2))
	    assert_equal(0..6, v.conv_point_range(0..-1))
	    assert_equal(4..6, v.conv_point_range(-2..-1))
    end

    def test_char_span
    	v = "ЁРШ ТВОЙУ МЕДДЬ".u.norm_D
	assert_equal("ЁРШ".u, v.char_span(0,3))
	assert_equal('\u0415\u0308\u0420'.u.unescape, v[0,3])
	assert_equal(v.norm_C, v.char_span(0,-1))
    end

    def test_sentinel_bug
    	("test" * 10).u.gsub(/e/.U, 'abracadabra'.u)
    end

    def test_string_change
    	a = " 123456789Aa ".u 
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.downcase!; m} }; 
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.upcase!; m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.lstrip!; m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.rstrip!; m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.strip!; m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.slice!(/Aa/.U); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.slice!("Aa".u); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.slice!(3,5); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.sub!(/Aa/.U, "BUG!".u); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.gsub!(/\d/.U) { |m|	a.gsub!(/Aa/.U, "BUG!".u); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.scan(/\d/.U) { |m|	a.gsub!(/Aa/.U, "BUG!".u); m} }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone
	assert_raise(RuntimeError) { 	a.each_char { |m|	a[2]= "BUG!".u } }
	assert_equal(" 123456789Aa ".u , a); 	a = a.clone

    end
    def test_1_to_u_to_s
	assert_equal(
		"\355\350\367\345\343\356 \355\345 \360\340\341\356\362\340\345\362 :( ?".to_u("cp-1251").to_s("utf-8"),
		"\320\275\320\270\321\207\320\265\320\263\320\276 \320\275\320\265 \321\200\320\260\320\261\320\276\321\202\320\260\320\265\321\202 :( ?")
   end

   def test_nested_blocks
   	a = "Модифицируемые строки иногда напрягают :)".u
	b = "".u
   	assert_nothing_raised {
		a.scan(/./.U) { |s|
			b = a.gsub(ure('и')) { |m| 
				t = m[0] + "".u
				a.each_char { |c|
			       		t << c if c == 'о' .u
			   	}
				t
			}
		}
	}
	assert_equal("Модиооофиоооциоооруемые строкиооо иоооногда напрягают :)".u, b)
   end
   
  def test_AREF # '[]'
    assert_equal(S("A"),  S("AooBar")[0])
    assert_equal(S("B"),  S("FooBaB")[-1])
    assert_equal(nil, S("FooBar")[6])
    assert_equal(nil, S("FooBar")[-7])

    assert_equal(S("Foo"), S("FooBar")[0,3])
    assert_equal(S("Bar"), S("FooBar")[-3,3])
    assert_equal(S(""),    S("FooBar")[6,2])
    assert_equal(nil,      S("FooBar")[-7,10])

    assert_equal(S("Foo"), S("FooBar")[0..2])
    assert_equal(S("Foo"), S("FooBar")[0...3])
    assert_equal(S("Bar"), S("FooBar")[-3..-1])
    assert_equal(S(""),    S("FooBar")[6..2])
    assert_equal(nil,      S("FooBar")[-10..-7])

    assert_equal(S("Foo"), S("FooBar")[/^F../.U])
    assert_equal(S("Bar"), S("FooBar")[/..r$/.U])
    assert_equal(nil,      S("FooBar")[/xyzzy/.U])
    assert_equal(nil,      S("FooBar")[/plugh/.U])

    assert_equal(S("Foo"), S("FooBar")[S("Foo")])
    assert_equal(S("Bar"), S("FooBar")[S("Bar")])
    assert_equal(nil,      S("FooBar")[S("xyzzy")])
    assert_equal(nil,      S("FooBar")[S("plugh")])

      assert_equal(S("Foo"), S("FooBar")[/([A-Z]..)([A-Z]..)/.U, 1])
      assert_equal(S("Bar"), S("FooBar")[/([A-Z]..)([A-Z]..)/.U, 2])
      assert_equal(nil,      S("FooBar")[/([A-Z]..)([A-Z]..)/.U, 3])
      assert_equal(S("Bar"), S("FooBar")[/([A-Z]..)([A-Z]..)/.U, -1])
      assert_equal(S("Foo"), S("FooBar")[/([A-Z]..)([A-Z]..)/.U, -2])
      assert_equal(nil,      S("FooBar")[ure("([A-Z]..)([A-Z]..)"), -3])
  end
  
  def test_ASET # '[]='
    s = S("FooBar")
    s[0] = S('A')
    assert_equal(S("AooBar"), s)

    s[-1]= S('B')
    assert_equal(S("AooBaB"), s)
    assert_raise(IndexError) { s[-7] = S("xyz") }
    assert_equal(S("AooBaB"), s)
    s[0] = S("ABC")
    assert_equal(S("ABCooBaB"), s)

    s = S("FooBar")
    s[0,3] = S("A")
    assert_equal(S("ABar"),s)
    s[0] = S("Foo")
    assert_equal(S("FooBar"), s)
    s[-3,3] = S("Foo")
    assert_equal(S("FooFoo"), s)
    assert_raise (IndexError) { s[7,3] =  S("Bar") }
    assert_raise (IndexError) { s[-7,3] = S("Bar") }

    s = S("FooBar")
    s[0..2] = S("A")
    assert_equal(S("ABar"), s)
    s[1..3] = S("Foo")
    assert_equal(S("AFoo"), s)
    s[-4..-4] = S("Foo")
    assert_equal(S("FooFoo"), s)
    assert_raise (RangeError) { s[7..10]   = S("Bar") }
    assert_raise (RangeError) { s[-7..-10] = S("Bar") }

    s = S("FooBar")
    s[/^F../.U]= S("Bar")
    assert_equal(S("BarBar"), s)
    s[/..r$/.U] = S("Foo")
    assert_equal(S("BarFoo"), s)
    
      s[/([A-Z]..)([A-Z]..)/.U, 1] = S("Foo")
      assert_equal(S("FooFoo"), s)
      s[/([A-Z]..)([A-Z]..)/.U, 2] = S("Bar")
      assert_equal(S("FooBar"), s)
      assert_raise (IndexError) { s[/([A-Z]..)([A-Z]..)/.U, 3] = "None" }
      s[ure("([A-Z]..)([A-Z]..)"), -1] = S("Foo")
      assert_equal(S("FooFoo"), s)
      s[/([A-Z]..)([A-Z]..)/.U, -2] = S("Bar")
      assert_equal(S("BarFoo"), s)
      #      assert_raise (IndexError) { s[/([A-Z]..)([A-Z]..)/.U, -3] = "None" }

    s = S("FooBar")
    s[S("Foo")] = S("Bar")
    assert_equal(S("BarBar"), s)

   s = S("a string")
    s[0..s.size] = S("another string")
    assert_equal(S("another string"), s)
  end
end
