require './icu4r'
require 'test/unit'
# these tests are ICU 3.4 dependent
class UCalendarTest < Test::Unit::TestCase
	
	def test_time_zones
		v = UCalendar.time_zones
		assert_kind_of(Array, v)
		assert_kind_of(UString, v[0])
    assert(v.include?("Europe/Kiev".u))
	end
	
	def test_default
		v = UCalendar.default_tz
		UCalendar.default_tz ="Europe/Paris".u
		assert_equal( "Europe/Paris".u, UCalendar.default_tz)
		c = UCalendar.new
    assert_equal( 3_600_000, c[:zone_offset])
		# assert_equal( "GMT+01:00".u, c.time_zone("root")) # this should work also
	end
	
	def test_dst
		assert_equal(UCalendar.dst_savings("America/Detroit".u), 3600000)
		assert_equal(UCalendar.dst_savings("Australia/Lord_Howe".u), 1800000)
	end

	def test_tz_for_country
	   zones = %w{Europe/Kiev Europe/Simferopol Europe/Uzhgorod Europe/Zaporozhye}.collect {|s| s.to_u}
	   assert_equal(zones, UCalendar.tz_for_country("UA"))
	end

	def test_time_now
		assert_equal(Time.now.to_i/100, UCalendar.now.to_i/100000)
	end

	def test_in_daylight
		t = UCalendar.new
		t.set_date(2006, 8, 22)
		t.time_zone = "US/Hawaii".u 
		assert_equal(false, t.in_daylight_time?)
		t.time_zone = "Europe/Berlin".u
		assert_equal(true, t.in_daylight_time?)
	end
	def test_set_date
		t = UCalendar.new
		t.set_date(2006, 0, 22)
		assert_equal(2006, t[:year])
		assert_equal(0,    t[:month])
		assert_equal(22,   t[:date])
		t[:year]  = 2007
		t[:month] = 2
		t[:date]  = 23
		assert_equal(2007, t[:year])
		assert_equal(2,    t[:month])
		assert_equal(23,   t[:date])

	end
	
	def test_set_date_time
		t = UCalendar.new
		t.set_date_time(2006, 0, 22, 11, 22, 33)
		assert_equal(11,  t[:hour])
		assert_equal(22,  t[:minute])
		assert_equal(33,  t[:second])
	end

	def test_millis
		m = UCalendar.now
		t = UCalendar.new
		assert(m <= t.millis)
		n = Time.now.to_i
		t.millis = n  * 1000.0
		assert_equal(n*1000.0, t.millis)
	end
	
	def test_add_time
		t = UCalendar.new
		t.set_date_time(2006, 0, 22, 11, 22, 33)
		t.add(:week_of_year, 1)
		assert_equal(29, t[:date])
		t.add(:hour, 48)
		assert_equal(31, t[:date])
	end
	
	def test_format
		t = UCalendar.new
		t.set_date_time(2006, 0, 22, 11, 22, 33)
		t.time_zone = "Europe/London".u
		assert_equal("2006/01/22 11:22:33 GMT AD".u,  t.format("yyyy/MM/dd HH:mm:ss z G".u, "en"))
	end

	def test_clone_and_compare
		c = UCalendar.new
		d = c.clone
		assert(c == d)
		assert(! (c < d) )
		assert(! (c > d) )
		assert(c.eql?(d))
		c.add(:date, 1)
		assert(c != d)
		assert(! (c < d) )
		assert( (c > d) )
		assert(!c.eql?(d))
		d.add(:date, 1)
		assert(c.eql?(d))
		d.time_zone = "Europe/Kiev".u
		assert(!c.eql?(d))
		assert(c == d)
	end
	
   def test_parse_date
	UCalendar.default_tz="UTC".u
        t1 = UCalendar.parse("HH:mm:ss E dd/MM/yyyy z".u, "en", "20:15:01 Fri 13/01/2006 GMT+00".u)
	assert_equal(2006, t1[:year])
	assert_equal(0, t1[:month])
	assert_equal(13, t1[:date])
	assert_equal(20, t1[:hour_of_day])
	assert_equal(15, t1[:minute])
	assert_equal(01, t1[:second])
    end


end
