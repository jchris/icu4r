#include "icu_common.h"
extern VALUE rb_cUString;
extern  VALUE icu_ustr_new_set(const UChar * str, long len, long capa);

/**
 * call-seq:
 *     ary.to_u => anUString
 *
 * Creates UString from array of fixnums, representing Unicode codepoints.
 * (inversion of UString#codepoints)
 *
 *      a = "поддержка".to_u.codepoints  # => [1087, 1086, 1076, 1076, 1077, 1088, 1078, 1082, 1072]
 *      a.to_u                           # => "поддержка"
 * 
 */
VALUE icu_ustr_from_array(obj)
	VALUE		obj;
{
	int		i, n;
	VALUE		*p;
	VALUE		ret, temp;
	UChar32		* src , *pos, chr;
	UChar		* buf;
	int32_t		len, capa;
	UErrorCode	status = U_ZERO_ERROR;

	n = RARRAY(obj)->len;
	p = RARRAY(obj)->ptr;
	
	src = ALLOC_N(UChar32, n);
	pos = src;
	for ( i = 0; i < n; i++){
	    temp = p[i];
	    if(TYPE(temp) != T_FIXNUM) {
	    	free(src);
	    	rb_raise(rb_eTypeError, "Can't convert from %s", rb_class2name(CLASS_OF(temp)));
	    }
	    chr = (UChar32) FIX2INT(temp);
	    // invalid codepoints are converted to U+FFFD
	    if( ! (U_IS_UNICODE_CHAR(chr)) ) {
	    	chr = 0xFFFD;
	    }
	    *pos = chr;
	    pos ++;
	}
	capa = n+1;
	buf = ALLOC_N(UChar, capa);
	u_strFromUTF32(buf, capa, &len, src, n, &status);
	if( U_BUFFER_OVERFLOW_ERROR == status ){
		capa = len+1;
		REALLOC_N(buf, UChar, capa);
		status = U_ZERO_ERROR;
		u_strFromUTF32(buf, capa, &len, src, n, &status);
	}
	if (U_FAILURE(status) ) {
		free(src);
		free(buf);
		rb_raise(rb_eRuntimeError, u_errorName(status));
	}
	if( capa <= len ){
		++capa;
		REALLOC_N(buf, UChar, capa);
	}
	ret = icu_ustr_new_set(buf, len, capa);
	free(src);
	return ret;
}

/**
 * call-seq:
 *    str.to_u(encoding = 'utf8') => String
 *
 * Converts String value in  given encoding to UString.
 * When no encoding is given, utf8 is assumed. If string is not valid UTF8,
 * and no encoding is given, exception is raised.
 *
 * When explicit encoding is given, converter will replace incorrect codepoints
 * with <U+FFFD> - replacement character.
 */
VALUE
icu_from_rstr(argc, argv, str)
     int             argc;
     VALUE          *argv,
                     str;
{
    VALUE           enc;
    char           *encoding = 0;	/* default */
    UErrorCode      error = 0;
    int32_t         capa, len;
    VALUE s;
    UChar * buf;
    UConverter * conv;
    if (rb_scan_args(argc, argv, "01", &enc) == 1) {
	Check_Type(enc, T_STRING);
	encoding = RSTRING(enc)->ptr;
    } 
    capa = RSTRING(str)->len + 1;
    buf = ALLOC_N(UChar, capa);

    if(! encoding || !strncmp(encoding, "utf8", 4) ) {
      /* from UTF8 */
        u_strFromUTF8(buf, capa-1, &len, RSTRING(str)->ptr, RSTRING(str)->len, &error);
	if( U_FAILURE(error)) {
	   free(buf);
	   rb_raise(rb_eArgError, u_errorName(error));
	}
	s = icu_ustr_new_set(buf, len, capa);
    } else {
          conv = ucnv_open(encoding, &error);
          if (U_FAILURE(error)) {
              ucnv_close(conv);
              rb_raise(rb_eArgError, u_errorName(error));
          }
          len =  ucnv_toUChars(conv, buf, capa-1, RSTRING(str)->ptr,
              	      RSTRING(str)->len, &error);
          if (U_BUFFER_OVERFLOW_ERROR == error) {
	      capa = len+1;
              REALLOC_N(buf, UChar, capa);
              error = 0;
              len = ucnv_toUChars(conv, buf, capa-1, RSTRING(str)->ptr,
              	      RSTRING(str)->len, &error);
              if (U_FAILURE(error)) {
		  free(buf);
                  rb_raise(rb_eArgError, u_errorName(error));
	      }
  
          }
          s = icu_ustr_new_set(buf, len, capa);
          ucnv_close(conv);
    }
    return s;
}

/**
 * call-seq:
 *   u(str, enc = 'utf8') => UString
 *
 * Global function to convert from String to UString
 */
VALUE
icu_f_rb_str(argc, argv, obj)
     int             argc;
     VALUE          *argv;
     VALUE           obj;
{
    VALUE           enc;
    VALUE           str;
    if (rb_scan_args(argc, argv, "11", &str, &enc) == 2) {
	Check_Type(enc, T_STRING);
    	Check_Type(str, T_STRING);
	return icu_from_rstr(1, &enc, str);
    } else {
    	Check_Type(str, T_STRING);
	return icu_from_rstr(0, NULL, str);
    }
    
}

void initialize_ucore_ext(void) 
{
    /* conversion from String to UString */
    rb_define_method(rb_cString, "to_u", icu_from_rstr, -1);
    rb_define_alias(rb_cString, "u", "to_u");
    rb_define_global_function("u", icu_f_rb_str, -1);

    /* conversion from Array to UString */
    rb_define_method(rb_cArray, "to_u", icu_ustr_from_array, 0);
}
