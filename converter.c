#include "icu_common.h"
extern VALUE rb_cUString;
extern VALUE icu_ustr_new_set(UChar * ptr, long len, long capa);
extern VALUE rb_cUConverter;

#define UCONVERTER(obj) ((UConverter *)DATA_PTR(obj))
 
static void icu4r_cnv_free(UConverter * conv)
{
    ucnv_close(conv);
}
static VALUE icu4r_cnv_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, 0, icu4r_cnv_free, 0);
}


/**
 * call-seq:
 *       conv = UConverter.new(name)
 *
 * Creates new converter, by given name. Name must be a Ruby String and may contain
 * additional options, e.g.:
 * 
 *        "SCSU,locale=ja"    # Converter option for specifying a locale
 *        "UTF-7,version=1"   # Converter option for specifying a version selector (0..9) for some converters.
 *        "ibm-1047,swaplfnl" # Converter option for EBCDIC SBCS or mixed-SBCS/DBCS (stateful) codepages.
 *
 * To get list of available converters call UConverter.list_available
 */
VALUE icu4r_cnv_init(VALUE self, VALUE name)
{
    UConverter * converter;
    UErrorCode status = U_ZERO_ERROR;
    
    Check_Type(name, T_STRING);
    converter = ucnv_open(RSTRING(name)->ptr,  &status);
    ICU_RAISE(status);
    DATA_PTR(self) = converter;
    return self;
} 
/**
 * call-seq:
 *     UConverter.list_available # => Array
 *
 *  Returns the names of available converters.
 */
VALUE icu4r_cnv_list(VALUE self) 
{
    VALUE ret ;
    int32_t count, i;
    count = ucnv_countAvailable();
    ret = rb_ary_new2(count);
    for( i = 0; i < count ; i++)
    {
       rb_ary_store(ret, i, rb_str_new2(ucnv_getAvailableName(i)));
    }
    return ret;
}

/**
 * call-seq:
 *     converter.subst_chars
 *
 * Returns substitution characters as multiple bytes
 */
VALUE icu4r_cnv_get_subst_chars(VALUE self)
{
    char buf[16];
    int8_t len = 16;
    UErrorCode status = U_ZERO_ERROR;
    ucnv_getSubstChars(UCONVERTER(self), buf, &len, &status);
    ICU_RAISE(status);
    return rb_str_new(buf, len);
}

/**
 * call-seq:
 *     converter.subst_chars=chars
 *
 * Sets the substitution chars when converting from unicode to a codepage.
 * The substitution is specified as a string of 1-4 bytes
 */
VALUE icu4r_cnv_set_subst_chars(VALUE self, VALUE str)
{
    UErrorCode status = U_ZERO_ERROR;
    Check_Type(str, T_STRING);
    ucnv_setSubstChars(UCONVERTER(self), RSTRING(str)->ptr, RSTRING(str)->len, &status);
    ICU_RAISE(status);
    return Qnil;
}

/**
 * call-seq:
 *     conv.name
 * 
 * Gets the internal, canonical name of the converter. 
 */
VALUE icu4r_cnv_name(VALUE self)
{
    UConverter * cnv = UCONVERTER(self);
    UErrorCode status = U_ZERO_ERROR;
    return rb_str_new2(ucnv_getName(cnv, &status));
}

/**
 * call-seq:
 *     converter.reset
 *
 * Resets the state of a converter to the default state.  
 * This is used in the case of an error, to restart a conversion from a known default state.
 * It will also empty the internal output buffers.
 */
VALUE icu4r_cnv_reset(VALUE self)
{
    UConverter * cnv = UCONVERTER(self);
    ucnv_reset(cnv);
    return Qnil;
} 

/** 
 * call-seq:
 *     conv.from_u(ustring) -> String 
 *
 * Convert the Unicode string into a codepage string using an existing UConverter.
 */
VALUE icu4r_cnv_from_unicode(VALUE self, VALUE str)
{
    UConverter * conv = UCONVERTER(self);
    UErrorCode status = U_ZERO_ERROR;
    int32_t enclen, capa;
    char * buf;
    VALUE s = Qnil;
    Check_Class(str, rb_cUString);
    capa = ICU_LEN(str) + 1;
    buf = ALLOC_N(char, capa);
    enclen = 	ucnv_fromUChars(conv, buf, capa-1, ICU_PTR(str), ICU_LEN(str), &status);
    if (U_BUFFER_OVERFLOW_ERROR == status) {
      REALLOC_N(buf, char, enclen + 1);
      status = 0;
      ucnv_fromUChars(conv, buf, enclen, ICU_PTR(str), ICU_LEN(str), &status);
    }
    if( U_FAILURE(status) ){
      free(buf);
      rb_raise(rb_eArgError, u_errorName(status));
    }
    s = rb_str_new(buf, enclen);
    return s;
}

/**
 * call-seq:
 *     conv.to_u(string) -> UString 
 *
 * Convert the codepage string into a Unicode string using an existing UConverter.
 */
VALUE icu4r_cnv_to_unicode(VALUE self, VALUE str) 
{
    UConverter * conv = UCONVERTER(self);
    UErrorCode status = U_ZERO_ERROR;
    long len, capa;
    VALUE s;
    UChar * buf;
    Check_Type(str, T_STRING);
    capa = RSTRING(str)->len + 1;
    buf = ALLOC_N(UChar, capa);
    len = ucnv_toUChars(conv, buf, capa-1, RSTRING(str)->ptr, RSTRING(str)->len, &status);
    if (U_BUFFER_OVERFLOW_ERROR == status) {
	      capa = len+1;
        REALLOC_N(buf, UChar, capa);
        status = 0;
        len = ucnv_toUChars(conv, buf, capa-1, RSTRING(str)->ptr, RSTRING(str)->len, &status);
        if (U_FAILURE(status)) {
          free(buf);
          rb_raise(rb_eArgError, u_errorName(status));
	      } 
    }
    s = icu_ustr_new_set(buf, len, capa);
    return s;
}

#define BUF_SIZE 1024
/**
 * call-seq:
 *     conv.convert(other_conv, string)
 *
 * Convert from one external charset to another using two existing UConverters,
 * ignoring the location of errors.
 */
VALUE icu4r_cnv_convert_to(VALUE self, VALUE other, VALUE src) 
{
   UConverter * cnv, * other_cnv;
   UErrorCode status = U_ZERO_ERROR;
   UChar pivotBuffer[BUF_SIZE];
   UChar *pivot, *pivot2;
   char * target,buffer[BUF_SIZE], *target_limit;
   const char * src_ptr, * src_end;
   VALUE ret;
   Check_Class(other, rb_cUConverter);
   Check_Type(src, T_STRING);
   pivot=pivot2=pivotBuffer;
   cnv = UCONVERTER(self);
   other_cnv = UCONVERTER(other);
   src_ptr = RSTRING(src)->ptr;
   src_end = src_ptr + RSTRING(src)->len;
   ret = rb_str_new2("");
   ucnv_reset(other_cnv);
   ucnv_reset(cnv);
   target_limit = buffer+BUF_SIZE;
   do {
     status = U_ZERO_ERROR;
     target = buffer;
     ucnv_convertEx( other_cnv, cnv, &target, target_limit,
        &src_ptr, src_end, pivotBuffer, &pivot, &pivot2, pivotBuffer+BUF_SIZE, FALSE, TRUE, &status);

     if(U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
       ICU_RAISE(status);
     }
     rb_str_buf_cat(ret, buffer, (int32_t)(target-buffer));
  } while (status == U_BUFFER_OVERFLOW_ERROR);
  return ret;
}

/** 
 * call-seq:
 *     UConverter.convert(to_converter_name, from_converter_name, source) # => String
 *
 * Convert from one external charset to another.
 * Internally, two converters are opened according to the name arguments, then the text is converted to and from using them.
 */
VALUE icu4r_cnv_convert(VALUE self, VALUE to_conv_name, VALUE from_conv_name, VALUE src)
{
     UErrorCode status = U_ZERO_ERROR;
     char * target = NULL;
     int32_t target_capa, len;
     VALUE ret;
     target_capa = ucnv_convert( RSTRING(to_conv_name)->ptr, RSTRING(from_conv_name)->ptr,
                   target, 0,
                   RSTRING(src)->ptr, RSTRING(src)->len, &status);
     if(status == U_BUFFER_OVERFLOW_ERROR){
        status = U_ZERO_ERROR;
        target_capa += 1;
        target = ALLOC_N(char, target_capa);
        len = ucnv_convert( RSTRING(to_conv_name)->ptr, RSTRING(from_conv_name)->ptr,
                   target, target_capa,
                   RSTRING(src)->ptr, RSTRING(src)->len, &status);
        if(U_FAILURE(status)){           
          free(target);
          ICU_RAISE(status);
        }
        ret = rb_str_new(target, len);
        free(target);
        return ret;
     } else ICU_RAISE(status);
     return rb_str_new2("");
}
/**
 * call-seq:
 *     UConverter.std_names(conv_name, std_name)
 *
 * Returns list of alias names for a given converter that are recognized by a standard; MIME and IANA are such standards
 */
VALUE icu4r_cnv_standard_names(VALUE self, VALUE cnv_name, VALUE std_name) 
{
	UEnumeration * name_list; 
  UErrorCode status = U_ZERO_ERROR;
	VALUE ret ;
	char * name;
	int32_t len;
  Check_Type(cnv_name, T_STRING);
  Check_Type(std_name, T_STRING);
	name_list = ucnv_openStandardNames(RSTRING(cnv_name)->ptr, RSTRING(std_name)->ptr, &status);
	ICU_RAISE(status);
	ret = rb_ary_new();
	while( (name = (char*)uenum_next(name_list, &len, &status))) {
		rb_ary_push(ret, rb_str_new2(name));
	}
	uenum_close(name_list);
	return ret;
}

/**
 * call-seq:
 *     UConverter.all_names
 *
 * Returns all of the canonical converter names, regardless of the ability to open each converter.
 */
VALUE icu4r_cnv_all_names(VALUE self) 
{
	UEnumeration * name_list; 
  UErrorCode status = U_ZERO_ERROR;
	VALUE ret ;
	char * name;
	int32_t len;
	name_list = ucnv_openAllNames(&status);
	ICU_RAISE(status);
	ret = rb_ary_new();
	while( (name = (char*)uenum_next(name_list, &len, &status))) {
		rb_ary_push(ret, rb_str_new2(name));
	}
	uenum_close(name_list);
	return ret;
}
void initialize_converter(void)
{
  rb_cUConverter = rb_define_class("UConverter", rb_cObject);
  rb_define_alloc_func(rb_cUConverter, icu4r_cnv_alloc);
  rb_define_method(rb_cUConverter, "initialize", icu4r_cnv_init, 1);

  rb_define_method(rb_cUConverter, "to_u", icu4r_cnv_to_unicode, 1);
  rb_define_method(rb_cUConverter, "from_u", icu4r_cnv_from_unicode, 1);
  rb_define_method(rb_cUConverter, "reset", icu4r_cnv_reset, 0);
  rb_define_method(rb_cUConverter, "name",  icu4r_cnv_name, 0);
  rb_define_method(rb_cUConverter, "convert", icu4r_cnv_convert_to, 2);
  rb_define_method(rb_cUConverter, "subst_chars=", icu4r_cnv_set_subst_chars, 1);
  rb_define_method(rb_cUConverter, "subst_chars",  icu4r_cnv_get_subst_chars, 0);
  rb_define_singleton_method(rb_cUConverter, "convert", icu4r_cnv_convert, 3);
  rb_define_singleton_method(rb_cUConverter, "list_available", icu4r_cnv_list, 0); 
  rb_define_singleton_method(rb_cUConverter, "std_names", icu4r_cnv_standard_names, 2);
  rb_define_singleton_method(rb_cUConverter, "all_names", icu4r_cnv_all_names, 0);
}

