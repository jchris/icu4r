#include "ruby.h"
#include "icu_common.h"
#include <unicode/msgfmt.h>
#include <unicode/translit.h>
#include <unicode/smpdtfmt.h>
#include <unicode/calendar.h>
#include <unicode/ucal.h>
/* This file contains various C-C++ wrappers, to ease my life
 */
extern "C" { 
extern  VALUE rb_cUString;
extern  VALUE rb_cUCalendar;
extern  VALUE icu_ustr_new(const UChar * str, long len);
extern  VALUE icu_ustr_new_set(const UChar * str, long len, long capa);

  VALUE icu_format(UChar * pattern, int32_t len, VALUE args, int32_t arg_len, char * locale)
{
  Formattable * arguments = new Formattable[arg_len];
  int i, is_set;
  VALUE obj;
  for(i = 0; i < arg_len; i++){
    obj = rb_ary_entry(args,i);
    is_set = 0;
    switch(TYPE(obj)){
      case T_FIXNUM:
      case T_FLOAT:
        arguments[i].setDouble(rb_num2dbl(obj));
        is_set = 1;
        break;
    }
    if(! is_set) {
       if (CLASS_OF(obj) == rb_cUString) {
          arguments[i].setString(UnicodeString(ICU_PTR(obj), ICU_LEN(obj)));
       } else 
         if (CLASS_OF(obj) == rb_cTime) {
           // ICU expects milliseconds since 01.01.1970
           arguments[i].setDate(rb_num2dbl(rb_funcall(obj, rb_intern("to_f"), 0))*1000);
         }
         else {
           delete [] arguments;
           rb_raise(rb_eArgError, "wrong arg type: %s", rb_class2name(CLASS_OF(obj)));
         }
    }
  }
  UnicodeString * patString = new UnicodeString(pattern,len);
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString * resultStr = new UnicodeString();
  FieldPosition * fieldPosition = new FieldPosition(0);
  Locale * loc = new Locale(locale);
  int32_t blen ;
  UChar * buf ; 
  VALUE ret ;

  MessageFormat * fmt= new MessageFormat(*patString,*loc, status);
  if( U_FAILURE(status) ){
	goto cleanup;	
  }
  fmt->format(arguments,arg_len,*resultStr,*fieldPosition,status);
  if( U_FAILURE(status) ){
	goto cleanup;	
  }
  blen = resultStr->length();
  buf = ALLOC_N(UChar, blen + 1);
  resultStr->extract(buf, blen, status);
  ret = icu_ustr_new( buf, blen);
  free(buf);
  
cleanup:
 	delete fmt;
	delete [] arguments;
	delete patString;
	delete resultStr;
	delete fieldPosition;
	delete loc;
  
	if( U_FAILURE(status) ){
 		rb_raise(rb_eArgError, "Can't format: %s", u_errorName(status));
	}else {
		return ret;
	}
}
UCalendar * icu_date_parse(UChar * str, int32_t str_len, char * locale, UChar * val, int32_t len) 
{
   UErrorCode status = U_ZERO_ERROR;
   UCalendar * c;
   c = ucal_open(NULL, -1, NULL,  UCAL_GREGORIAN, &status);
   if( U_FAILURE(status) ) {
     rb_raise(rb_eArgError, u_errorName(status));
   }
   UnicodeString * temp = new UnicodeString(str, str_len); 
   Locale * loc = new Locale(locale);
   SimpleDateFormat * formatter = new SimpleDateFormat(*temp, *loc, status);
   if( U_FAILURE(status) ) {
     delete formatter;
     delete temp;
     delete loc;
     rb_raise(rb_eArgError, "Can't create formatter:%s", u_errorName(status));
   }
   formatter->setLenient( 0 );
   UnicodeString * val_str = new UnicodeString(val, len);
   UDate p_time = formatter->parse(*val_str, status);
   ucal_setMillis(c, p_time, &status);
     delete formatter;
     delete temp;
     delete loc;
     delete val_str;

   if( U_FAILURE(status) ) {
     ucal_close(c);
     rb_raise(rb_eArgError, "Can't parse date:%s", u_errorName(status));
   }
   return c;
}
VALUE icu_transliterate(UChar * str, int32_t str_len, UChar * id, int32_t id_len, UChar * rules, int32_t rule_len)
{
	UErrorCode    status = U_ZERO_ERROR;
	UParseError   p_error;
	Transliterator * t  ;
	if( rules != NULL)  {
	  t = Transliterator::createFromRules(UnicodeString(id, id_len), UnicodeString(rules, rule_len), 
	  	UTRANS_FORWARD, p_error, status);
	} else {
	  t = Transliterator::createInstance(UnicodeString(id, id_len), UTRANS_FORWARD, p_error, status);
	}
	if( U_FAILURE(status) ) 
	{
	   rb_raise(rb_eRuntimeError, u_errorName(status));
	}
	UnicodeString * src = new UnicodeString(str, str_len);
	t->transliterate(*src);
	int32_t blen = src->length();
	UChar * buf = ALLOC_N(UChar, blen + 1);
	src->extract(buf, blen, status);
	VALUE ret = icu_ustr_new_set( buf, blen, blen+1);
	delete src;
	delete t;
	return ret;
}
extern void icu4r_cal_free(UCalendar *);

VALUE icu4r_cal_clone(VALUE cal) 
{
	Calendar * clon;
	clon = ((Calendar *)(DATA_PTR(cal)))->clone();
	return Data_Wrap_Struct(rb_cUCalendar, 0, icu4r_cal_free, clon); 
}
#define CPP_CALENDAR(obj)  ((Calendar*)DATA_PTR(obj))

VALUE icu4r_cal_equal(VALUE cal, VALUE obj) 
{
	UBool		answer;
	Check_Class( obj, rb_cUCalendar);
	answer = (*CPP_CALENDAR(cal)) == (*CPP_CALENDAR(obj));
	return answer ? Qtrue : Qfalse;	
}
}
