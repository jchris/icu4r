require './icu4r'
require 'test/unit'
# these tests are ICU 3.4 dependent
class UConverterTest < Test::Unit::TestCase
  
  def test_a_new_and_name
    c = UConverter.new("UTF8")
    assert_kind_of( UConverter, c)
    assert_equal('UTF-8', c.name)
  end

  def test_b_list_avail
    a = UConverter.list_available
    assert_kind_of(Array, a)
    assert(a.include?("UTF-8"))
  end
  
  def test_c_all_names
    a = UConverter.all_names
    assert_kind_of(Array, a)
    assert(a.include?("UTF-8"))
  end
  
  def test_d_std_names
    a = UConverter.std_names("koi8r", "MIME")
    assert_kind_of(Array, a)
    assert(a.include?("KOI8-R"))
    a = UConverter.std_names("cp1251", "IANA")
    assert_kind_of(Array, a)
    assert(a.include?("windows-1251"))
  end

  def test_e_convert_class_method
    a_s = "\357\360\356\342\345\360\352\340 abcd" 
    u_s = UConverter.convert("utf8", "cp1251", a_s)
    assert_equal("проверка abcd", u_s)
    r_s = UConverter.convert("cp1251", "utf8", u_s)
    assert_equal(r_s, a_s)
  end

  def test_f_to_from_u
    c = UConverter.new("cp1251")
    a_s = "\357\360\356\342\345\360\352\340 abcd" 
    u_s = c.to_u(a_s)
    assert_kind_of(UString, u_s)
    r_s = c.from_u(u_s)
    assert_equal(r_s, a_s)
  end

  def test_g_convert_instance_method
    c1 = UConverter.new("EUC-JP")
    c2 = UConverter.new("Cp1251")
    a_s = "\247\322\247\335\247\361!"
    b_s = a_s * 1000
    a1 = UConverter.convert("Cp1251", "EUC-JP", b_s)
    a2 = c1.convert(c2,  b_s)
    assert_equal(a1.size, a2.size)
    assert_equal(a2.size, 4 * 1000)
    assert_equal(a1, a2)
    assert_equal("\341\353\377!", c1.convert(c2, a_s))
  end

  def test_h_subst_chars
    c1 = UConverter.new("US-ASCII")
    assert_kind_of(String, c1.subst_chars)
    c1.subst_chars="!"
    assert_equal( "I!t!rn!ti!n!liz!ti!n", c1.from_u("Iñtërnâtiônàlizætiøn".u))
    c1.subst_chars=" "
    assert_equal( "I t rn ti n liz ti n", c1.from_u("Iñtërnâtiônàlizætiøn".u))
  end
  
end
