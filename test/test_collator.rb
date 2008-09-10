require './icu4r'
require 'test/unit'
# these tests are ICU 3.4 dependent
class UCollatorTest < Test::Unit::TestCase
  def test_strength
    c = UCollator.new("root")
    assert_equal(0,  c.strcoll("a".u, "a".u))
    assert_equal(1,  c.strcoll("A".u, "a".u))
    c.strength = UCollator::UCOL_SECONDARY
    assert_equal(0,  c.strcoll("A".u, "a".u))
  end
  
  def test_attrs
   c = UCollator.new("root")
   c[UCollator::UCOL_NUMERIC_COLLATION]= UCollator::UCOL_ON
   ar = %w(100 10 20 30 200 300).map {|a| a.to_u }.sort {|a,b| c.strcoll(a,b)}.map {|s| s.to_s }
   assert_equal(["10", "20", "30", "100", "200", "300"], ar)
   c[UCollator::UCOL_NUMERIC_COLLATION]= UCollator::UCOL_OFF
   ar = %w(100 10 20 30 200 300).map {|a| a.to_u }.sort {|a,b| c.strcoll(a,b)}.map {|s| s.to_s }
   assert_equal( ["10", "100", "20", "200", "30", "300"], ar)
  end
  
  def test_sort_key
   c = UCollator.new("root")
   c[UCollator::UCOL_NUMERIC_COLLATION]= UCollator::UCOL_ON
   ar = %w(100 10 20 30 200 300).sort_by {|a| c.sort_key(a.to_u) }
   assert_equal(["10", "20", "30", "100", "200", "300"], ar)
   c[UCollator::UCOL_NUMERIC_COLLATION]= UCollator::UCOL_OFF
   ar = %w(100 10 20 30 200 300).sort_by {|a| c.sort_key(a.to_u) }
   assert_equal( ["10", "100", "20", "200", "30", "300"], ar)
  end

end
