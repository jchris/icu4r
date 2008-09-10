#include "icu_common.h"
extern VALUE rb_cUString;
extern VALUE rb_cUCollator;
extern int icu_collator_cmp (UCollator * collator, VALUE str1, VALUE str2) ;

/**
 * Document-class: UCollator
 *
 * API for UCollator performs locale-sensitive string comparison. You use this service to build searching and 
 * sorting routines for natural language text.
 * 
 * Attributes that collation service understands:
 * 
 *     UCOL_FRENCH_COLLATION    Attribute for direction of secondary weights - used in French. UCOL_ON, UCOL_OFF
 *     
 *     UCOL_ALTERNATE_HANDLING  Attribute for handling variable elements. UCOL_NON_IGNORABLE (default), UCOL_SHIFTED 
 *     
 *     UCOL_CASE_FIRST          Controls the ordering of upper and lower case letters. 
 *                              UCOL_OFF (default),  UCOL_UPPER_FIRST, UCOL_LOWER_FIRST
 *     
 *     UCOL_CASE_LEVEL          Controls whether an extra case level (positioned before the third level) is 
 *                              generated or not. UCOL_OFF (default), UCOL_ON
 *                              
 *     UCOL_NORMALIZATION_MODE  Controls whether the normalization check and necessary normalizations are performed.  
 *                              When set to UCOL_ON, an incremental check is performed to see whether the input data 
 *                              is in the FCD form. If the data is not in the FCD form, incremental NFD normalization 
 *                              is performed.
 *                              
 *     UCOL_DECOMPOSITION_MODE  An alias for UCOL_NORMALIZATION_MODE attribute
 *     
 *     UCOL_STRENGTH            The strength attribute. 
 *                              Can be either UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_QUATERNARY, UCOL_IDENTICAL. 
 *                              The usual strength for most locales (except Japanese) is tertiary. 
 *    
 *     UCOL_HIRAGANA_QUATERNARY_MODE  when turned on, this attribute positions Hiragana before all non-ignorables on 
 *                                    quaternary level This is a sneaky way to produce JIS sort order
 *     UCOL_NUMERIC_COLLATION   when turned on, this attribute generates a collation key for the numeric value of 
 *                              substrings of digits. This is a way to get '100' to sort AFTER '2'. 
 *                          
 * Attribute values:
 * 
 *      UCOL_DEFAULT           accepted by most attributes
 *      UCOL_PRIMARY           Primary collation strength
 *      UCOL_SECONDARY         Secondary collation strength
 *      UCOL_TERTIARY          Tertiary collation strength
 *      UCOL_DEFAULT_STRENGTH  Default collation strength
 *      UCOL_QUATERNARY        Quaternary collation strength
 *      UCOL_IDENTICAL         Identical collation strength
 *      UCOL_OFF               Turn the feature off - works for 
 *                             UCOL_FRENCH_COLLATION, UCOL_CASE_LEVEL, 
 *                             UCOL_HIRAGANA_QUATERNARY_MODE & UCOL_DECOMPOSITION_MODE
 *                             
 *      UCOL_ON                Turn the feature on - works for UCOL_FRENCH_COLLATION, UCOL_CASE_LEVEL, 
 *                             UCOL_HIRAGANA_QUATERNARY_MODE & UCOL_DECOMPOSITION_MODE
 *                             
 *      UCOL_SHIFTED           Valid for UCOL_ALTERNATE_HANDLING. Alternate handling will be shifted
 *      UCOL_NON_IGNORABLE     Valid for UCOL_ALTERNATE_HANDLING. Alternate handling will be non ignorable
 *      UCOL_LOWER_FIRST       Valid for UCOL_CASE_FIRST - lower case sorts before upper case
 *      UCOL_UPPER_FIRST       upper case sorts before lower case
 **/

#define UCOLLATOR(obj) ((UCollator *)DATA_PTR(obj))
 
void icu4r_col_free(UCollator * col)
{
    ucol_close(col);
}
static VALUE icu4r_col_alloc(VALUE klass)
{
    return Data_Wrap_Struct(klass, 0, icu4r_col_free, 0);
}
/**
 * call-seq:
 *       col = UCollator.new(locale = nil)
 *       
 * Open a UCollator for comparing strings for the given locale containing the required collation rules. 
 * Special values for locales can be passed in - if +nil+ is passed for the locale, the default locale 
 * collation rules will be used. If empty string ("") or "root" are passed, UCA rules will be used.
 */
VALUE icu4r_col_init(int argc, VALUE * argv, VALUE self)
{
    UCollator * col;
    UErrorCode status = U_ZERO_ERROR;
    VALUE  loc;
    char * locale = NULL;
	  if( rb_scan_args(argc, argv, "01", &loc)) 
    {
	     Check_Type(loc, T_STRING);
       locale = RSTRING(loc)->ptr;
    }
    col = ucol_open(locale,  &status);
    ICU_RAISE(status);
    DATA_PTR(self)=col;
    return self;
}

/**
 * call-seq:
 *     collator.strength 
 *
 * Get the collation strength used in a UCollator. The strength influences how strings are compared.
 **/
VALUE icu4r_col_get_strength(VALUE self)
{
    return INT2NUM(ucol_getStrength(UCOLLATOR(self)));
}

/**
 * call-seq:
 *     collator.strength = new_strength
 *
 * Sets the collation strength used in a UCollator. The strength influences how strings are compared.
 **/
VALUE icu4r_col_set_strength(VALUE self, VALUE obj)
{
    Check_Type(obj, T_FIXNUM);
    ucol_setStrength(UCOLLATOR(self), FIX2INT(obj));
    return Qnil;
}

/**
 * call-seq:
 *     collator.get_attr(attribute)
 *     collator[attribute]
 *
 * Universal attribute setter. See above for valid attributes and their values
 **/
VALUE icu4r_col_get_attr(VALUE self, VALUE obj)
{
    UErrorCode status = U_ZERO_ERROR;
    UColAttributeValue val;
    Check_Type(obj, T_FIXNUM);
    val = ucol_getAttribute(UCOLLATOR(self), FIX2INT(obj), &status);
    ICU_RAISE(status);
    return INT2FIX(val);
}

/**
 * call-seq:
 *     collator.set_attr(attribute, value)
 *     collator[attribute]=value 
 * 
 * Universal attribute setter. See above for valid attributes and their values
 **/
VALUE icu4r_col_set_attr(VALUE self, VALUE obj, VALUE new_val)
{
    UErrorCode status = U_ZERO_ERROR;
    Check_Type(obj, T_FIXNUM);
    Check_Type(new_val, T_FIXNUM);
    ucol_setAttribute(UCOLLATOR(self), FIX2INT(obj), FIX2INT(new_val), &status);
    ICU_RAISE(status);
    return Qnil;
}
/**
 * call-seq:
 *     collator.strcoll(ustr1, ustr2)
 *
 * Compare two UString's. The strings will be compared using the options already specified.
 **/
VALUE icu4r_col_strcoll(VALUE self, VALUE str1, VALUE str2)
{
    Check_Class(str1, rb_cUString);
    Check_Class(str2, rb_cUString);
    return INT2FIX(icu_collator_cmp(UCOLLATOR(self), str1,  str2));
}
/**
 * call-seq:
 *     collator.sort_key(an_ustring) -> String
 *
 * Get a sort key for a string from a UCollator. Sort keys may be compared using strcmp.
 **/
VALUE icu4r_col_sort_key(VALUE self, VALUE str)
{
    int32_t needed , capa ;
    char * buffer ; 
    VALUE ret;
    Check_Class(str, rb_cUString);
    capa = ICU_LEN(str);
    buffer = ALLOC_N(char, capa);
    needed = ucol_getSortKey(UCOLLATOR(self), ICU_PTR(str), ICU_LEN(str), buffer, capa);
    if(needed > capa){
      REALLOC_N(buffer,char, needed);
      needed = ucol_getSortKey(UCOLLATOR(self), ICU_PTR(str), ICU_LEN(str), buffer, needed);
    }
    ret = rb_str_new(buffer, needed);
    free(buffer);
    return ret;
}
void initialize_collator()
{
  rb_cUCollator = rb_define_class("UCollator", rb_cObject);
  rb_define_alloc_func(rb_cUCollator, icu4r_col_alloc);

  rb_define_method(rb_cUCollator, "initialize", icu4r_col_init, -1);
  rb_define_method(rb_cUCollator, "strength",  icu4r_col_get_strength, 0);
  rb_define_method(rb_cUCollator, "strength=", icu4r_col_set_strength, 1);
  rb_define_method(rb_cUCollator, "get_attr",  icu4r_col_get_attr, 1);
  rb_define_alias(rb_cUCollator, "[]", "get_attr");
  rb_define_method(rb_cUCollator, "set_attr",  icu4r_col_set_attr, 2);
  rb_define_alias(rb_cUCollator, "[]=", "set_attr");
  rb_define_method(rb_cUCollator, "strcoll", icu4r_col_strcoll, 2);
  rb_define_method(rb_cUCollator, "sort_key",icu4r_col_sort_key, 1);
  
  /* attributes */
  rb_define_const(rb_cUCollator, "UCOL_FRENCH_COLLATION", INT2FIX(UCOL_FRENCH_COLLATION));
  rb_define_const(rb_cUCollator, "UCOL_ALTERNATE_HANDLING", INT2FIX(UCOL_ALTERNATE_HANDLING));
  rb_define_const(rb_cUCollator, "UCOL_CASE_FIRST", INT2FIX(UCOL_CASE_FIRST)); 
  rb_define_const(rb_cUCollator, "UCOL_CASE_LEVEL", INT2FIX(UCOL_CASE_LEVEL));
  rb_define_const(rb_cUCollator, "UCOL_NORMALIZATION_MODE", INT2FIX(UCOL_NORMALIZATION_MODE)); 
  rb_define_const(rb_cUCollator, "UCOL_DECOMPOSITION_MODE", INT2FIX(UCOL_DECOMPOSITION_MODE));
  rb_define_const(rb_cUCollator, "UCOL_STRENGTH", INT2FIX(UCOL_STRENGTH));
  rb_define_const(rb_cUCollator, "UCOL_HIRAGANA_QUATERNARY_MODE", INT2FIX(UCOL_HIRAGANA_QUATERNARY_MODE));
  rb_define_const(rb_cUCollator, "UCOL_NUMERIC_COLLATION", INT2FIX(UCOL_NUMERIC_COLLATION));
  rb_define_const(rb_cUCollator, "UCOL_ATTRIBUTE_COUNT", INT2FIX(UCOL_ATTRIBUTE_COUNT));

  /* attribute values */
  rb_define_const(rb_cUCollator, "UCOL_DEFAULT", INT2FIX(UCOL_DEFAULT));
  rb_define_const(rb_cUCollator, "UCOL_PRIMARY", INT2FIX(UCOL_PRIMARY));
  rb_define_const(rb_cUCollator, "UCOL_SECONDARY", INT2FIX(UCOL_SECONDARY));
  rb_define_const(rb_cUCollator, "UCOL_TERTIARY", INT2FIX(UCOL_TERTIARY));
  rb_define_const(rb_cUCollator, "UCOL_DEFAULT_STRENGTH", INT2FIX(UCOL_DEFAULT_STRENGTH));
  rb_define_const(rb_cUCollator, "UCOL_CE_STRENGTH_LIMIT", INT2FIX(UCOL_CE_STRENGTH_LIMIT)); 
  rb_define_const(rb_cUCollator, "UCOL_QUATERNARY", INT2FIX(UCOL_QUATERNARY));
  rb_define_const(rb_cUCollator, "UCOL_IDENTICAL", INT2FIX(UCOL_IDENTICAL));
  rb_define_const(rb_cUCollator, "UCOL_STRENGTH_LIMIT", INT2FIX(UCOL_STRENGTH_LIMIT)); 
  rb_define_const(rb_cUCollator, "UCOL_OFF", INT2FIX(UCOL_OFF)); 
  rb_define_const(rb_cUCollator, "UCOL_ON", INT2FIX(UCOL_ON));
  rb_define_const(rb_cUCollator, "UCOL_SHIFTED", INT2FIX(UCOL_SHIFTED));
  rb_define_const(rb_cUCollator, "UCOL_NON_IGNORABLE", INT2FIX(UCOL_NON_IGNORABLE));
  rb_define_const(rb_cUCollator, "UCOL_LOWER_FIRST", INT2FIX(UCOL_LOWER_FIRST));
  rb_define_const(rb_cUCollator, "UCOL_UPPER_FIRST", INT2FIX(UCOL_UPPER_FIRST));
  
}
