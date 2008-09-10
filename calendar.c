#include <unicode/ucal.h>
#include <unicode/uenum.h>
#include <unicode/udat.h>
#include "icu_common.h"
extern VALUE rb_cUString;
extern VALUE icu_ustr_new(UChar * ptr, long len);
extern VALUE icu_ustr_new_set(UChar * ptr, long len, long capa);
static VALUE s_calendar_fields, s_calendar_formats;
extern VALUE rb_cUCalendar;
#define UCALENDAR(obj) ((UCalendar *)DATA_PTR(obj))
/**
 * Document-class: UCalendar
 *
 * The UCalendar class  provides methods for converting between a specific instant in 
 * time and a set of calendar fields such as YEAR, MONTH, DAY_OF_MONTH, HOUR, and so on,
 * and for manipulating the calendar fields, such as getting the date of the next week. 
 * An instant in time can be represented by a millisecond value that is an offset from the 
 * Epoch, January 1, 1970 00:00:00.000 GMT (Gregorian).
 *
 * Also timezone info and methods are provided.
 *
 * NOTE: months are zero-based.
 */

/**
 * call-seq:
 *     UCalendar.time_zones
 *
 * Returns array with all time zones (as UString values). 
 */
VALUE icu4r_cal_all_tz (VALUE obj)
{
	UErrorCode  status = U_ZERO_ERROR;
	UEnumeration * zones ; 
	VALUE ret ;
	UChar * name;
	int32_t len;
	zones = ucal_openTimeZones (&status);
	ICU_RAISE(status);
	ret = rb_ary_new();
	while( (name = (UChar*)uenum_unext(zones, &len, &status))) {
		rb_ary_push(ret, icu_ustr_new(name, len));
	}
	uenum_close(zones);
	return ret;
}


/**
 * call-seq:
 *     UCalendar.tz_for_country(country)
 *  
 * Returns array with all time zones associated with the given country.
 * Note: <code>country</code> must be value of type String.
 * Returned array content is UString's
 *
 *     UCalendar.tz_for_country("GB") # => [ "Europe/Belfast", "Europe/London", "GB",  "GB-Eire"]
 *
 */
VALUE icu4r_cal_country_tz (VALUE obj, VALUE ctry)
{
	UErrorCode  status = U_ZERO_ERROR;
	UEnumeration * zones ; 
	VALUE ret ;
	UChar * name;
	int32_t len;
	Check_Type(ctry, T_STRING);
	zones = ucal_openCountryTimeZones (RSTRING(ctry)->ptr, &status) ;
	ICU_RAISE(status);
	ret = rb_ary_new();
	while( (name = (UChar*)uenum_unext(zones, &len, &status))) {
		rb_ary_push(ret, icu_ustr_new(name, len));
	}
	uenum_close(zones);
	return ret;
}

/**
 * call-seq:
 *     UCalendar.default_tz => ustring
 *
 * Returns the default time zone name as UString.
 *
 *     UCalendar.default_tz # "EET"
 *
 */
VALUE icu4r_cal_get_default_tz(VALUE obj) 
{
	UErrorCode  status = U_ZERO_ERROR;
	UChar * buf ;
	long capa = 0;
	capa = ucal_getDefaultTimeZone (buf, capa, &status);
	if( U_BUFFER_OVERFLOW_ERROR == status) {
		buf = ALLOC_N(UChar, capa+1);
		status = U_ZERO_ERROR;
		capa = ucal_getDefaultTimeZone (buf, capa, &status);
		return icu_ustr_new_set(buf, capa, capa+1);
	}
	ICU_RAISE(status);
	return Qnil;
}

/** 
 * call-seq:
 *     UCalendar.default_tz = ustring
 *
 * Set the default time zone.
 *
 *      UCalendar.default_tz="GMT+00".u
 *      UCalendar.default_tz="Europe/Paris".u
 */
VALUE icu4r_cal_set_default_tz(VALUE obj, VALUE tz) 
{
	UErrorCode  status = U_ZERO_ERROR;
	Check_Class(tz, rb_cUString);
  	ucal_setDefaultTimeZone (ICU_PTR(tz), &status);	
	ICU_RAISE(status);
	return tz;
}
/**
 * call-seq:
 *      UCalendar.dst_savings(zone_id)
 *
 * Return the amount of time in milliseconds that the clock is advanced 
 * during daylight  savings time for the given time zone, or zero if the time 
 * zone does not observe daylight savings time. 
 *
 *       UCalendar.dst_savings("GMT+00".u) # =>  3600000
 *
 */
VALUE icu4r_cal_dst_savings(VALUE obj, VALUE zone) 
{
	UErrorCode  status = U_ZERO_ERROR;
	int32_t dst;
	Check_Class(zone, rb_cUString);
  	dst = ucal_getDSTSavings (ICU_PTR(zone), &status);
	ICU_RAISE(status);
	return INT2FIX(dst);
}

/**
 * call-seq:
 *     UCalendar.now
 *
 * Get the current date and time. in millis
 *
 *       UCalendar.now # => 1137934561000.0
 *
 */
VALUE icu4r_cal_now(VALUE obj){
	return rb_float_new(ucal_getNow());
}
//-----------------

void icu4r_cal_free(UCalendar * cal){
    ucal_close(cal);
}
static VALUE icu4r_cal_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, 0, icu4r_cal_free, 0);
}
/**
 * call-seq:
 *     UCalendar.new(zone_id = nil, locale = nil, traditional = false)
 *
 * Creates new instance of UCalendar, for given time zone id (UString),
 * locale (Ruby String), and kind , by default gregorian.
 * New instance has current time.
 *
 */
VALUE icu4r_cal_init (int argc, VALUE * argv, VALUE self)
{
         VALUE zone, loc, cal_type;
	 UChar * zone_id = NULL;
	 char  * locale  = NULL;
	 UCalendarType  c_type = UCAL_GREGORIAN;
	 int32_t n, zone_len =0 , locale_len =0;
	 UCalendar * calendar;
	 UErrorCode  status = U_ZERO_ERROR;
	 n = rb_scan_args(argc, argv, "03", &zone, &loc, &cal_type);
	 if( n >= 1) {
	     Check_Class(zone, rb_cUString);
	     zone_id = ICU_PTR(zone);
	     zone_len = ICU_LEN(zone);
	 }
	 if( n >= 2) {
	     Check_Type(loc, T_STRING);
	     locale = RSTRING(loc)->ptr;
	     locale_len = RSTRING(loc)->len;
	 }
	 if( n >= 3) {
	     if( Qtrue == cal_type ) {
	     	c_type = UCAL_TRADITIONAL;
	     }
	 }
	 calendar = ucal_open(zone_id, zone_len, locale,  c_type, &status);
	 ICU_RAISE(status);
	 DATA_PTR(self) = calendar;
	 return self;
}

int icu4r_get_cal_field_int(VALUE field)
{
	VALUE  field_const;
	field_const = rb_hash_aref(s_calendar_fields, field);
	if(field_const == Qnil)
	rb_raise(rb_eArgError, "no  such field %s", rb_obj_as_string(field));
	return NUM2INT(field_const);
}

/** 
 * call-seq:
 *     calendar.add(field, int_amount)
 *
 * Add a specified signed amount to a particular field in a UCalendar.
 *
 *      c.add(:week_of_year, 2)
 */
VALUE icu4r_cal_add(VALUE obj, VALUE field, VALUE amount) 
{
	UErrorCode status = U_ZERO_ERROR;
	int  date_field;
	Check_Type(field, T_SYMBOL);
	Check_Type(amount, T_FIXNUM);
	date_field = icu4r_get_cal_field_int(field);
	ucal_add(UCALENDAR(obj), date_field, FIX2INT(amount) , &status); 
	ICU_RAISE(status);
	return Qnil;
}

/** 
 * call-seq:
 *     calendar.roll(field, int_amount)
 *
 * Adds a signed amount to the specified calendar field without changing larger fields. 
 * A negative roll amount means to subtract from field without changing larger fields. 
 * If the specified amount is 0, this method performs nothing.
 *
 * Example: Consider a GregorianCalendar originally set to August 31, 1999. Calling roll(:month, 8) 
 * sets the calendar to April 30, 1999. Using a Gregorian Calendar, the :day_of_month field cannot 
 * be 31 in the month April. :day_of_month is set to the closest possible value, 30. The :year field 
 * maintains the value of 1999 because it is a larger field than :month. 
 */
VALUE icu4r_cal_roll(VALUE obj, VALUE field, VALUE amount) 
{
	UErrorCode status = U_ZERO_ERROR;
	int  date_field;
	Check_Type(field, T_SYMBOL);
	Check_Type(amount, T_FIXNUM);
	date_field = icu4r_get_cal_field_int(field);
	ucal_roll(UCALENDAR(obj), date_field, FIX2INT(amount) , &status); 
	ICU_RAISE(status);
	return Qnil;
}
/** 
 * call-seq:
 *     calendar[field]
 *
 * Get the current value of a field from a UCalendar.
 *
 */
VALUE icu4r_cal_aref(VALUE obj, VALUE field) 
{
	UErrorCode status = U_ZERO_ERROR;
	int  date_field;
	int32_t value;
	Check_Type(field, T_SYMBOL);
	date_field = icu4r_get_cal_field_int(field);
	value = ucal_get(UCALENDAR(obj), date_field, &status); 
	ICU_RAISE(status);
	return INT2FIX(value);
}

/** 
 * call-seq:
 *    calendar[field]=int_value
 *
 * Set the value of a field in a UCalendar.
 */
VALUE icu4r_cal_aset(VALUE obj, VALUE field, VALUE amount) 
{
	int  date_field, ret;
	UErrorCode status = U_ZERO_ERROR;
	Check_Type(field, T_SYMBOL);
	Check_Type(amount, T_FIXNUM);
	date_field = icu4r_get_cal_field_int(field);
	ucal_set(UCALENDAR(obj), date_field, FIX2INT(amount) ); 
	ret = ucal_get(UCALENDAR(obj), date_field, &status); 

	return INT2FIX(ret);
}
/**
 * call-seq:
 *     calendar.millis
 *
 * return this calendar value in milliseconds.
 */
VALUE icu4r_cal_millis(VALUE obj)
{
	UErrorCode status = U_ZERO_ERROR;
	double  millis;
	millis = ucal_getMillis(UCALENDAR(obj), &status); 
	ICU_RAISE(status);
	return rb_float_new(millis);
}
/**
 * call-seq:
 *     calendar.millis = new_value
 *
 * Sets calendar's value in milliseconds.
 */
VALUE icu4r_cal_set_millis(VALUE obj,VALUE milli)
{
	UErrorCode status = U_ZERO_ERROR;
	Check_Type(milli, T_FLOAT);
	ucal_setMillis(UCALENDAR(obj), rb_num2dbl(milli), &status); 
	ICU_RAISE(status);
	return Qnil;
}

/**
 * call-seq:
 *     calendar.set_date(year, month, date)
 *
 * Set a UCalendar's current date.
 */
VALUE icu4r_cal_set_date(VALUE obj,VALUE year, VALUE mon, VALUE date)
{
	UErrorCode status = U_ZERO_ERROR;
	Check_Type(year, T_FIXNUM);
	Check_Type(mon,  T_FIXNUM);
	Check_Type(date, T_FIXNUM);
	ucal_setDate(UCALENDAR(obj), FIX2INT(year), FIX2INT(mon), FIX2INT(date), &status); 
	ICU_RAISE(status);
	return Qnil;
}
/**
 * call-seq:
 *     calendar.set_date_time(year, month, date, hour, minute, second)
 *
 * Set a UCalendar's current date and time.
 */
VALUE icu4r_cal_set_date_time(VALUE obj,VALUE year, VALUE mon, VALUE date, VALUE hour, VALUE min, VALUE sec)
{
	UErrorCode status = U_ZERO_ERROR;
	Check_Type(year, T_FIXNUM);
	Check_Type(mon,  T_FIXNUM);
	Check_Type(date, T_FIXNUM);
	Check_Type(hour, T_FIXNUM);
	Check_Type(min,  T_FIXNUM);
	Check_Type(sec, T_FIXNUM);

	ucal_setDateTime(UCALENDAR(obj), 
		FIX2INT(year), FIX2INT(mon), FIX2INT(date), 
		FIX2INT(hour), FIX2INT(min), FIX2INT(sec), 
	&status); 
	ICU_RAISE(status);
	return Qnil;
}

/** 
 * call-seq:
 *     calendar.time_zone = zone_id
 *
 * Set the TimeZone used by a UCalendar. 
 */
VALUE icu4r_cal_set_tz(VALUE obj, VALUE zone)
{
	UErrorCode status = U_ZERO_ERROR;
	Check_Class(zone, rb_cUString);
	ucal_setTimeZone(UCALENDAR(obj), ICU_PTR(zone), ICU_LEN(zone), &status);
	ICU_RAISE(status);
	return Qnil;

}

/**
 * call-seq: 
 *      calendar.in_daylight_time?
 *
 * Determine if a UCalendar is currently in daylight savings time.
 *
 * Daylight savings time is not used in all parts of the world
 */
VALUE icu4r_cal_in_daylight(VALUE obj)
{
	UErrorCode status = U_ZERO_ERROR;
	int32_t	answer;
	answer = ucal_inDaylightTime(UCALENDAR(obj), &status);
	ICU_RAISE(status);
	return answer ? Qtrue : Qfalse;
}

/** 
 * call-seq:
 *     calendar.time_zone(locale = nil) 
 *
 * Returns the TimeZone name used in this UCalendar. Name is returned in requested locale or default, if not set.
 */
VALUE icu4r_cal_get_tz (int argc, VALUE * argv, VALUE obj)
{
	UErrorCode  status = U_ZERO_ERROR;
	UChar * buf  = NULL;
	long capa = 0;
	char *locale = NULL;
	VALUE loc;
	if( rb_scan_args(argc, argv, "01", &loc) == 1){
		Check_Type(loc, T_STRING);
		locale = RSTRING(loc)->ptr;
	}
	
	capa = ucal_getTimeZoneDisplayName(UCALENDAR(obj), UCAL_STANDARD, locale, buf, capa, &status);
	if( U_BUFFER_OVERFLOW_ERROR == status) {
		buf = ALLOC_N(UChar, capa+1);
		status = U_ZERO_ERROR;
		capa = ucal_getTimeZoneDisplayName(UCALENDAR(obj), UCAL_STANDARD, locale, buf, capa, &status);
		return icu_ustr_new_set(buf, capa, capa+1);
	}
	ICU_RAISE(status);
	return Qnil;

}
int icu4r_get_cal_format_int(VALUE field)
{
	VALUE  field_const;
	field_const = rb_hash_aref(s_calendar_formats, field);
	if(field_const == Qnil)	{
		rb_warn("no  such format %s , using default", RSTRING(rb_obj_as_string(field))->ptr);
		return UDAT_DEFAULT;
	}
	return NUM2INT(field_const);
}
/** call-seq:
 *     calendar.format(pattern = nil , locale = nil)
 *
 * Formats this calendar time using given pattern and locale. Returns UString or nil on failure.
 * Valid value types for pattern are:
 *      nil        - long format for date and time
 *      UString    - specification of format, as defined in docs/FORMATTING
 *      Symbol     - one of :short, :medium, :long, :full, :none , sets format for both date and time
 *      Hash       - {:time => aSymbol, :date => aSymbol} - sets separate formats for date and time, valid symbols see above
 */
VALUE icu4r_cal_format(int argc, VALUE * argv, VALUE obj) 
{
	UErrorCode status = U_ZERO_ERROR;
	UDateFormat * format;
	UDate   time_to_format;
	UChar * buf = NULL, * pattern = NULL;
	long capa = 0, pattern_len = 0;
	char *locale = NULL;
	VALUE loc, pat, ret = Qnil;
	int n , def_d_format = UDAT_FULL, def_t_format = UDAT_FULL;
	
	n = rb_scan_args(argc, argv, "02", &pat, &loc);
	if( n == 2) {
		Check_Type(loc, T_STRING);
		locale = RSTRING(loc)->ptr;
	}
	if (n >= 1 && pat != Qnil) {
		switch(TYPE(pat)) {
			case T_SYMBOL:
			 	def_d_format = def_t_format = icu4r_get_cal_format_int(pat);
				break;
			case T_HASH:
			 	def_d_format = icu4r_get_cal_format_int(rb_hash_aref(pat, ID2SYM(rb_intern("date"))));
				def_t_format = icu4r_get_cal_format_int(rb_hash_aref(pat, ID2SYM(rb_intern("time"))));
				break;
			default:
				Check_Class(pat, rb_cUString);
				pattern = ICU_PTR(pat);
				pattern_len = ICU_LEN(pat);
				break;
		}
	}
	
	format = udat_open(def_t_format, def_d_format, locale, NULL, 0,  NULL, 0, &status);
	if( pattern ) {
	   udat_applyPattern(format, 0, pattern, pattern_len);
	}
	ICU_RAISE(status);
	udat_setCalendar(format, UCALENDAR(obj));
	time_to_format = ucal_getMillis(UCALENDAR(obj), &status); 

	capa = udat_format(format, time_to_format, buf, capa, NULL, &status);
	if( U_BUFFER_OVERFLOW_ERROR == status) {
		buf = ALLOC_N(UChar, capa+1);
		status = U_ZERO_ERROR;
		capa = udat_format(format, time_to_format, buf, capa, NULL, &status);
		ret = icu_ustr_new_set(buf, capa, capa+1);
	}
	udat_close(format);
	ICU_RAISE(status);
	return ret;
}

/**
 * Document-method: clone
 *
 * call-seq:
 *     cal.clone => UCalendar
 *
 * Create and return a copy of this calendar.
 */
extern VALUE icu4r_cal_clone(VALUE obj);
/**
 * Document-method: eql?
 * 
 * call-seq:
 *	cal.eql?(other)
 *
 * Compares the equality of two UCalendar objects.
 * 
 * This comparison is very exacting; two UCalendar objects must be in exactly the 
 * same state to be considered equal.
 */
extern VALUE icu4r_cal_equal(VALUE obj, VALUE other);

/**
 *  call-seq:
 *     cal <=> other_cal   => -1, 0, +1
 *  
 *  Comparison---Returns -1 if <i>other_cal</i> is before than, 0 if
 *  <i>other_cal</i> is equal to, and +1 if <i>other_cal</i> is after than
 *  <i>str</i>. 
 *
 *  Value of calendar's milliseconds are compared.
 */

VALUE icu4r_cal_cmp (VALUE c1, VALUE c2) 
{
	UErrorCode status = U_ZERO_ERROR;
	double  millis1, millis2;
	Check_Class(c1, rb_cUCalendar);
	Check_Class(c2, rb_cUCalendar);
	millis1 = ucal_getMillis(UCALENDAR(c1), &status); 
	millis2 = ucal_getMillis(UCALENDAR(c2), &status); 
	ICU_RAISE(status);
	if(millis1 < millis2) return INT2FIX(-1);
	if(millis1 > millis2) return INT2FIX(1);
	return INT2FIX(0);
}
/* parsing     */
extern UCalendar * icu_date_parse(UChar * str, int32_t str_len, char * locale, UChar * val, int32_t len);

/**
 * call-seq:
 *     UCalendar.parse( pattern, locale, value)
 *  
 * Parses given  value, using format pattern with respect to +locale+.
 *
 *     UCalendar.parse("HH:mm:ss E dd/MM/yyyy".u, "en", "20:15:01 Fri 13/01/2006".u) # => Time.local(2006,"jan",13,20,15,1) 
 *
 */	
 
VALUE 
icu4r_cal_parse( obj, str,  locale, val)
		VALUE obj, str, locale, val;
{
   UCalendar 		* cal;
   VALUE 		ret;
   Check_Type(locale, T_STRING);
   Check_Class(val, rb_cUString);
   cal = icu_date_parse(ICU_PTR(str), ICU_LEN(str), RSTRING(locale)->ptr, ICU_PTR(val), ICU_LEN(val));
      ret = Data_Wrap_Struct(obj, 0, icu4r_cal_free, cal);
   return ret;
}

void initialize_calendar(void) {

rb_cUCalendar = rb_define_class("UCalendar", rb_cObject);
rb_define_alloc_func(rb_cUCalendar, icu4r_cal_alloc);

s_calendar_fields = rb_hash_new();
/* Valid symbols to use as field reference in UCalendar#[],  UCalendar#[]=, UCalendar#add are:
 :era , :year , :month , :week_of_year , :week_of_month , :date , :day_of_year , :day_of_week,  :day_of_week_in_month, 
 :am_pm , :hour , :hour_of_day , :minute , :second , :millisecond , :zone_offset , :dst_offset: 
*/
rb_define_const(rb_cUCalendar, "UCALENDAR_FIELDS", s_calendar_fields);
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("era")), INT2NUM(UCAL_ERA ));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("year")), INT2NUM(UCAL_YEAR ));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("month")), INT2NUM(UCAL_MONTH));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("week_of_year")), INT2NUM(UCAL_WEEK_OF_YEAR ));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("week_of_month")), INT2NUM(UCAL_WEEK_OF_MONTH));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("date")), INT2NUM(UCAL_DATE));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("day_of_year")), INT2NUM(UCAL_DAY_OF_YEAR));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("day_of_week")), INT2NUM(UCAL_DAY_OF_WEEK));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("day_of_week_in_month")), INT2NUM(UCAL_DAY_OF_WEEK_IN_MONTH));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("am_pm")), INT2NUM(UCAL_AM_PM));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("hour")), INT2NUM(UCAL_HOUR));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("hour_of_day")), INT2NUM(UCAL_HOUR_OF_DAY));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("minute")), INT2NUM(UCAL_MINUTE ));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("second")), INT2NUM(UCAL_SECOND));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("millisecond")), INT2NUM(UCAL_MILLISECOND ));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("zone_offset")), INT2NUM(UCAL_ZONE_OFFSET));
rb_hash_aset(s_calendar_fields, ID2SYM(rb_intern("dst_offset")), INT2NUM(UCAL_DST_OFFSET));

s_calendar_formats = rb_hash_new();
rb_define_const(rb_cUCalendar, "UCALENDAR_FORMATS", s_calendar_formats);
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("full")), INT2NUM(UDAT_FULL));
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("long")), INT2NUM(UDAT_LONG));
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("medium")), INT2NUM(UDAT_MEDIUM));
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("short")), INT2NUM(UDAT_SHORT));
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("default")), INT2NUM(UDAT_DEFAULT));
rb_hash_aset(s_calendar_formats, ID2SYM(rb_intern("none")), INT2NUM(UDAT_NONE));


rb_define_singleton_method(rb_cUCalendar, "now", icu4r_cal_now, 0);

rb_define_singleton_method(rb_cUCalendar, "default_tz=", icu4r_cal_set_default_tz, 1);
rb_define_singleton_method(rb_cUCalendar, "default_tz",  icu4r_cal_get_default_tz, 0);
rb_define_singleton_method(rb_cUCalendar, "time_zones",  icu4r_cal_all_tz, 0);
rb_define_singleton_method(rb_cUCalendar, "tz_for_country",  icu4r_cal_country_tz, 1);
rb_define_singleton_method(rb_cUCalendar, "dst_savings",  icu4r_cal_dst_savings, 1);
rb_define_singleton_method(rb_cUCalendar, "parse",  icu4r_cal_parse, 3);

rb_define_method(rb_cUCalendar, "initialize", icu4r_cal_init, -1);
rb_define_method(rb_cUCalendar, "add", icu4r_cal_add, 2);
rb_define_method(rb_cUCalendar, "roll", icu4r_cal_roll, 2);
rb_define_method(rb_cUCalendar, "[]", icu4r_cal_aref, 1);
rb_define_method(rb_cUCalendar, "[]=", icu4r_cal_aset, 2);
rb_define_method(rb_cUCalendar, "millis=", icu4r_cal_set_millis, 1);
rb_define_method(rb_cUCalendar, "millis", icu4r_cal_millis,0);
rb_define_method(rb_cUCalendar, "set_date", icu4r_cal_set_date,3);
rb_define_method(rb_cUCalendar, "set_date_time", icu4r_cal_set_date_time,6);
rb_define_method(rb_cUCalendar, "time_zone=", icu4r_cal_set_tz,1);
rb_define_method(rb_cUCalendar, "time_zone", icu4r_cal_get_tz,-1);
rb_define_method(rb_cUCalendar, "in_daylight_time?", icu4r_cal_in_daylight,0);
rb_define_method(rb_cUCalendar, "format", icu4r_cal_format,-1);

rb_define_method(rb_cUCalendar, "clone", icu4r_cal_clone,0);
rb_define_method(rb_cUCalendar, "eql?", icu4r_cal_equal,1);
rb_define_method(rb_cUCalendar, "<=>", icu4r_cal_cmp,1);

rb_include_module(rb_cUCalendar, rb_mComparable);

}	
