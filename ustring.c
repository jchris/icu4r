/**
 *  ustring.c - ICU based Unicode string support.
 * 
 * $Id: ustring.c,v 1.20 2006/01/23 14:26:45 meadow Exp $
 *
 * Copyright (c) 2006 Nikolai Lugovoi
 *
 * This code is based on original ruby String class source (string.c):
 *
 *  * string.c -
 *  *
 *  * Copyright (C) 1993-2003 Yukihiro Matsumoto
 *  * Copyright (C) 2000  Network Applied Communication Laboratory, Inc.
 *  * Copyright (C) 2000  Information-technology Promotion Agency, Japan
 *  *
 **/  

#include "icu_common.h"
VALUE           icu_ustr_replace(VALUE str, VALUE str2);
VALUE		ustr_gsub(int argc, VALUE * argv, VALUE str, int bang, int once);
extern VALUE icu_from_rstr(int argc, VALUE * argv, VALUE str);

 VALUE rb_cURegexp;
 VALUE rb_cUString;
 VALUE rb_cUMatch;
 VALUE rb_cUResourceBundle;
 VALUE rb_cULocale;
 VALUE rb_cUCalendar;
 VALUE rb_cUConverter;
 VALUE rb_cUCollator;
 
#include "uregex.h"


/* to be used in <=>, casecmp */
static UCollator * s_UCA_collator, * s_case_UCA_collator;

static void
free_ustr(str)
     ICUString      *str;
{
    if (str->ptr)
	free(str->ptr);
    str->ptr = 0;
    free(str);
}
inline void icu_check_frozen(int check_busy, VALUE str)
{
	rb_check_frozen(str);
	if(check_busy && USTRING(str)->busy > 0 ) rb_raise(rb_eRuntimeError, "String is busy. Can't modify");
}
#define START_BUF_LEN  16
/**
 * Allocate ICUString struct with given +capa+ capacity,
 * if mode == 1 and UChar != 0 - copy len UChars from src,
 * else set pointer to src.
 */
#define   ICU_COPY   1
#define   ICU_SET    0
VALUE icu_ustr_alloc_and_wrap(UChar * src, long len, long capa, int mode) 
{
    ICUString      *n_str = ALLOC_N(ICUString, 1);
    size_t 		alloc_capa;
    if( mode == ICU_COPY ) {
    	alloc_capa = START_BUF_LEN > capa ? START_BUF_LEN : capa;
	if(alloc_capa<=len) alloc_capa = len + 1;
    	n_str->ptr = ALLOC_N(UChar, alloc_capa);
	n_str->capa = alloc_capa;
    	n_str->len = len;
	if( src ) {
		u_memcpy(n_str->ptr, src, len);  
		n_str->ptr[len] = 0;
	} 
    } else {
    	n_str->ptr = src;
	n_str->len = len;
	n_str->capa = capa;
    }
     if(n_str->capa <= n_str->len) rb_raise(rb_eRuntimeError, "Capacity is not large then len, sentinel can't be set!");
    n_str->busy = 0;
    n_str->ptr[n_str->len] = 0;
    return Data_Wrap_Struct(rb_cUString, 0, free_ustr, n_str);
}
VALUE
icu_ustr_alloc(klass)
     VALUE           klass;
{
	return icu_ustr_alloc_and_wrap(NULL, 0, 0, ICU_COPY);
}
void ustr_capa_resize(ICUString * str, long new_capa)
{
    if (new_capa != str->capa) {
	if (str->capa < new_capa || (str->capa - new_capa > 1024)) {
	    if(new_capa < START_BUF_LEN) new_capa = START_BUF_LEN;
	    REALLOC_N(str->ptr, UChar, new_capa);
	    str->capa = new_capa;
	}
    }
}
/* delete +del_len+ units from string and insert replacement */
void ustr_splice_units(ICUString * str, long start, long del_len, const UChar * replacement, long repl_len)
{
   long new_len;
   UChar * temp  = 0 ;
   if( str->busy ) {
   	rb_warn("Attempt to modify busy string. Ignored");
	return;
   }
   if( repl_len < 0) return;
   if( del_len == 0 && repl_len == 0) return;
   new_len = str->len - del_len + repl_len;
   if (replacement == str->ptr ) { 
       temp = ALLOC_N(UChar, repl_len);
       u_memcpy(temp, replacement, repl_len);
       replacement = temp;
   }
   if ( repl_len >= del_len) ustr_capa_resize(str, new_len+1);
   /* move tail */
   if(str->len - (start+del_len) > 0) { 
      u_memmove(str->ptr + start+repl_len, str->ptr + start+del_len, str->len-(start+del_len) );
   }
   /* copy string */
   if( repl_len > 0) u_memcpy(str->ptr+start, replacement, repl_len);
   if ( repl_len < del_len)  ustr_capa_resize(str, new_len+1);
   str->len = new_len;
   str->ptr[new_len] = 0;
   if(temp) {
     free(temp);
   }
}
static inline void
ustr_mod_check(VALUE s, UChar *p, long len)
{
    if (ICU_PTR(s) != p || ICU_LEN(s) != len){
	rb_raise(rb_eRuntimeError, "string modified");
    }
}
VALUE
ustr_new(klass, ptr, len)
     VALUE           klass;
     UChar    *ptr;
     long            len;
{   
    if (len < 0) {
	rb_raise(rb_eArgError, "negative string size (or size too big)");
    }
    return icu_ustr_alloc_and_wrap(ptr, len, len+1, ICU_COPY);
}

VALUE
icu_ustr_new(ptr, len)
     const UChar    *ptr;
     long            len;
{
    return ustr_new(rb_cUString, ptr, len);
}
VALUE
icu_ustr_new_set(ptr, len, capa)
     UChar    *ptr;
     long            len;
     long 	     capa;
{
	return icu_ustr_alloc_and_wrap(ptr, len, capa, ICU_SET);
}
VALUE
icu_ustr_new2(ptr)
     const UChar    *ptr;
{
    if (!ptr) {
	rb_raise(rb_eArgError, "NULL pointer given");
    }
    return icu_ustr_new(ptr, u_strlen(ptr));
}

inline VALUE
icu_ustr_new_capa(UChar * ptr, long len, long capa) 
{
	return icu_ustr_alloc_and_wrap(ptr, len, capa, ICU_COPY);
}

/* ------------ */

/**
 *  call-seq:
 *     UString.new(str="".u)   => new_str
 *  
 *  Returns a new string object containing a copy of <i>str</i>.
 */

VALUE
icu_ustr_init(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           orig;

    if (rb_scan_args(argc, argv, "01", &orig) == 1)
    {
	icu_ustr_replace(str, orig);
    }
    return str;
}

/**
 *  call-seq:
 *     str.length  => integer
 *  
 *  Returns the length of <i>str</i>.
 */
VALUE
icu_ustr_length(str)
     VALUE           str;
{
    return LONG2NUM(ICU_LEN(str));
}

/**
 *  call-seq:
 *     str.empty?   => true or false
 *  
 *  Returns <code>true</code> if <i>str</i> has a length of zero.
 *     
 *     "hello".u.empty?   #=> false
 *     "".u.empty?        #=> true
 */

VALUE
icu_ustr_empty(str)
     VALUE           str;
{
    return 0 == ICU_LEN(str) ? Qtrue : Qfalse;
}

VALUE
icu_ustr_resize(str, len)
     VALUE           str;
     long            len;
{
    if (len < 0) {
	rb_raise(rb_eArgError, "negative string size (or size too big)");
    }
    ustr_capa_resize(USTRING(str), len);
    ICU_LEN(str) = len;
    ICU_PTR(str)[len] = 0;	/* sentinel */
    return str;
}


/**
 *  call-seq:
 *     str.replace(other_str)   => str
 *  
 *  Replaces the contents and taintedness of <i>str</i> with the corresponding
 *  values in <i>other_str</i>.
 *     
 *     s = "hello".u       #=> "hello"
 *     s.replace "world".u   #=> "world"
 */
VALUE
icu_ustr_replace(str, str2)
     VALUE           str,
                     str2;
{
    if (str == str2)
	return str;
    icu_check_frozen(1, str);	
    Check_Class(str2, rb_cUString);
    ustr_splice_units(USTRING(str), 0, ICU_LEN(str), ICU_PTR(str2), ICU_LEN(str2));
    OBJ_INFECT(str, str2);
    return str;
}

/**
 *  call-seq:
 *     string.clear    ->  string
 *
 *  Makes string empty.
 *
 *     a = "abcde".u
 *     a.clear    #=> ""
 */

VALUE
icu_ustr_clear(str)
     VALUE           str;
{
    icu_check_frozen(1, str);
    icu_ustr_resize(str, 0);
    return str;
}

int icu_collator_cmp (UCollator * collator, VALUE str1, VALUE str2) 
{
    int  ret = 0,  result ;
    result = ucol_strcoll(collator, ICU_PTR(str1), ICU_LEN(str1), ICU_PTR(str2), ICU_LEN(str2));
    switch(result){
	  case UCOL_EQUAL:   ret = 0;break;
	  case UCOL_GREATER: ret = 1;break;
	  case UCOL_LESS:    ret = -1;break;
    }
    return ret;
}

int
icu_ustr_cmp(str1, str2)
     VALUE           str1,
                     str2;
{
	return icu_collator_cmp(s_UCA_collator, str1, str2);
}

/**
 *  call-seq:
 *     str == obj   => true or false
 *
 *  Equality---If <i>obj</i> is not a <code>UString</code>, returns
 *  <code>false</code>. Otherwise, returns <code>true</code> if
 *  strings are of the same length and content
 *  
 */

VALUE
icu_ustr_equal(str1, str2)
     VALUE           str1,
                     str2;
{
    if (str1 == str2)
	return Qtrue;
    if (CLASS_OF(str2) != rb_cUString) {
	return Qfalse;
    }
    if (ICU_LEN(str1) == ICU_LEN(str2) && 
		    u_strncmp(ICU_PTR(str1), ICU_PTR(str2), ICU_LEN(str1) ) == 0) {
	return Qtrue;
    }
    return Qfalse;
}

/**
 *  call-seq:
 *     str <=> other_str   => -1, 0, +1
 *  
 *  Comparison---Returns -1 if <i>other_str</i> is less than, 0 if
 *  <i>other_str</i> is equal to, and +1 if <i>other_str</i> is greater than
 *  <i>str</i>. 
 *  
 *  <code><=></code> is the basis for the methods <code><</code>,
 *  <code><=</code>, <code>></code>, <code>>=</code>, and <code>between?</code>,
 *  included from module <code>Comparable</code>.  The method
 *  <code>String#==</code> does not use <code>Comparable#==</code>.
 *
 *  This method uses UCA rules, see also #strcoll for locale-specific string collation.
 *     
 *     "abcdef".u <=> "abcde".u     #=> 1
 *     "abcdef".u <=> "abcdef".u    #=> 0
 *     "abcdef".u <=> "abcdefg".u   #=> -1
 *     "abcdef".u <=> "ABCDEF".u    #=> -1
 */

VALUE
icu_ustr_cmp_m(str1, str2)
     VALUE           str1,
                     str2;
{
    long            result;

    if (CLASS_OF(str2) != rb_cUString) {
	return Qnil;
    } else {
	result = icu_ustr_cmp(str1, str2);
    }
    return LONG2NUM(result);
}

/**
 *  call-seq:
 *     str.casecmp(other_str)   => -1, 0, +1
 *  
 *  Case-insensitive version of <code>UString#<=></code> .
 *  This method uses UCA collator with secondary strength, see #strcoll
 *  
 *     
 *     "abcdef".u.casecmp("abcde".u)     #=> 1
 *     "aBcDeF".u.casecmp("abcdef".u)    #=> 0
 *     "abcdef".u.casecmp("abcdefg".u)   #=> -1
 *     "abcdef".u.casecmp("ABCDEF".u)    #=> 0
 */

VALUE
icu_ustr_casecmp(str1, str2)
     VALUE           str1,
                     str2;
{
    Check_Class(str2, rb_cUString);
    return INT2FIX(icu_collator_cmp(s_case_UCA_collator, str1, str2)); 
}

/**
 *  call-seq:
 *     str + other_str   => new_str
 *  
 *  Concatenation---Returns a new <code>UString</code> containing
 *  <i>other_str</i> concatenated to <i>str</i>.
 *     
 *     "Hello from ".u + "main".u   #=> "Hello from main"
 */

VALUE
icu_ustr_plus(str1, str2)
     VALUE           str1,
                     str2;
{
    VALUE           str3;
    Check_Class(str2, rb_cUString);

    str3 = icu_ustr_new_capa(ICU_PTR(str1), ICU_LEN(str1), ICU_LEN(str1) + ICU_LEN(str2));
    ustr_splice_units(USTRING(str3), ICU_LEN(str3), 0, ICU_PTR(str2), ICU_LEN(str2));
    if (OBJ_TAINTED(str1) || OBJ_TAINTED(str2))
	OBJ_TAINT(str3);
    return str3;
}

/**
 *  call-seq:
 *     str * integer   => new_str
 *  
 *  Copy---Returns a new <code>UString</code> containing <i>integer</i> copies of
 *  the receiver.
 *     
 *     "Ho! ".u * 3   #=> "Ho! Ho! Ho! ".u
 */

VALUE
icu_ustr_times(str, times)
     VALUE           str,
                     times;
{
    VALUE           str2;
    long            i,
                    len;
    Check_Type(times, T_FIXNUM);
    len = NUM2LONG(times);
    if (len < 0) {
	rb_raise(rb_eArgError, "negative argument");
    }
    if (len && LONG_MAX / len < ICU_LEN(str)) {
	rb_raise(rb_eArgError, "argument too big");
    }

    str2 = icu_ustr_new_capa(0, 0, len *= ICU_LEN(str));
    for (i = 0; i < len; i += ICU_LEN(str)) {
	ustr_splice_units(USTRING(str2), i, 0, ICU_PTR(str), ICU_LEN(str));
    }
    ICU_PTR(str2)[ICU_LEN(str2)] = 0;

    OBJ_INFECT(str2, str);

    return str2;
}


/**
 *  call-seq:
 *     str << other_str           => str
 *     str.concat(other_str)      => str
 *  
 *  Append---Concatenates the given string object to <i>str</i>.
 *     
 *     a = "hello ".u
 *     a << "world".u   #=> "hello world"
 */

VALUE
icu_ustr_concat(str1, str2)
     VALUE           str1,
                     str2;
{
    icu_check_frozen(1, str1);
    Check_Class(str2, rb_cUString);
    if (ICU_LEN(str2) > 0) {
	ustr_splice_units(USTRING(str1), ICU_LEN(str1), 0, ICU_PTR(str2), ICU_LEN(str2));
	OBJ_INFECT(str1, str2);
    }
    return str1;
}

int
icu_ustr_hash(str)
     VALUE           str;
{
    register long   len = ICU_LEN(str) * (sizeof(UChar));
    register char *p = (char*)ICU_PTR(str);
    register int    key = 0;

    while (len--) {
	key += *p++;
	key += (key << 10);
	key ^= (key >> 6);
    }
    key += (key << 3);
    key ^= (key >> 11);
    key += (key << 15);
    return key;
}

/**
 * call-seq:
 *    str.hash   => fixnum
 *
 * Return a hash based on the string's length and content.
 */

VALUE
icu_ustr_hash_m(str)
     VALUE           str;
{
    int             key = icu_ustr_hash(str);
    return INT2FIX(key);
}

VALUE
icu_ustr_dup(str)
     VALUE           str;
{
    VALUE           dup = icu_ustr_new(ICU_PTR(str), ICU_LEN(str));
    return dup;
}

/**
 *  call-seq:
 *     str.upcase!(locale = "")   => str or nil
 *  
 *  Upcases the contents of <i>str</i>, returning <code>nil</code> if no changes
 *  were made. This method is locale-sensitive.
 */

VALUE
icu_ustr_upcase_bang(argc, argv, str)
     int argc;
     VALUE * argv;
     VALUE           str;

{
    UErrorCode      error = 0;
    UChar          *buf = 0; 
    long            len ;
    VALUE           loc;
    char *	    locale = NULL;
    icu_check_frozen(1, str);
    buf = ALLOC_N(UChar, ICU_LEN(str) + 1);
    if (rb_scan_args(argc, argv, "01", &loc) == 1) {
       if( loc != Qnil) {
         Check_Type(loc, T_STRING);
	 locale = RSTRING(loc)->ptr;
       }
    }

    len = u_strToUpper(buf, ICU_LEN(str), ICU_PTR(str), ICU_LEN(str), locale, &error);
    if (U_BUFFER_OVERFLOW_ERROR == error) {
	REALLOC_N(buf, UChar, len + 1);
	error = 0;
	len =
	    u_strToUpper(buf, len, ICU_PTR(str), ICU_LEN(str), locale, &error);
    }
    if (0 == u_strncmp(buf, ICU_PTR(str), len))
	return Qnil;
    free(ICU_PTR(str));
    ICU_PTR(str) = buf;
    ICU_LEN(str) = len;
    return str;
}


/**
 *  call-seq:
 *     str.upcase(locale = "")   => new_str
 *  
 *  Returns a copy of <i>str</i> with all lowercase letters replaced with their
 *  uppercase counterparts. The operation is locale sensitive.
 *     
 *     "hEllO".u.upcase   #=> "HELLO"
 */

VALUE
icu_ustr_upcase(argc, argv, str)
     int argc;
     VALUE * argv;
     VALUE           str;

{
    str = icu_ustr_dup(str);
    icu_ustr_upcase_bang(argc, argv, str);
    return str;
}


/**
 *  call-seq:
 *     str.downcase!(locale = "")   => str or nil
 *  
 *  Downcases the contents of <i>str</i>, returning <code>nil</code> if no
 *  changes were made.
 */

VALUE
icu_ustr_downcase_bang(argc, argv, str)
     int argc;
     VALUE * argv;
     VALUE           str;
{
    UErrorCode      error = 0;
    UChar          *buf;
    long            len ;
    VALUE           loc;
    char *	    locale = NULL;
    buf = ALLOC_N(UChar, ICU_LEN(str) + 1);
    icu_check_frozen(1, str);
    if (rb_scan_args(argc, argv, "01", &loc) == 1) {
       if( loc != Qnil) {
         Check_Type(loc, T_STRING);
	 locale = RSTRING(loc)->ptr;
       }
    }
    len = 
	u_strToLower(buf, ICU_LEN(str), ICU_PTR(str), ICU_LEN(str), locale,
		     &error);
    if (U_BUFFER_OVERFLOW_ERROR == error) {
	REALLOC_N(buf, UChar, len + 1);
	error = 0;
	len =
	    u_strToLower(buf, len , ICU_PTR(str), ICU_LEN(str), locale,
			 &error);
    }
    if (0 == u_strncmp(buf, ICU_PTR(str), len))
	return Qnil;
    free(ICU_PTR(str));
    ICU_PTR(str) = buf;
    ICU_LEN(str) = len;
    return str;
}

/**
 *  call-seq:
 *     str.downcase(locale = "")   => new_str
 *  
 *  Returns a copy of <i>str</i> with all uppercase letters replaced with their
 *  lowercase counterparts. The operation is locale sensitive.
 *     
 *     "hEllO".u.downcase   #=> "hello"
 */

VALUE
icu_ustr_downcase(argc, argv, str)
     int argc;
     VALUE * argv;
     VALUE           str;
{
    str = icu_ustr_dup(str);
    icu_ustr_downcase_bang(argc, argv, str);
    return str;
}

/**
 * call-seq:
 *     str.foldcase
 *
 * Case-fold the characters in a string.
 * Case-folding is locale-independent and not context-sensitive.
 *
 */
VALUE
icu_ustr_foldcase(str)
     VALUE           str;
{
    UErrorCode      error = 0;
    UChar          *buf;
    long            len, capa ;
    capa = ICU_LEN(str) + 1;
    buf = ALLOC_N(UChar, capa);
    len = u_strFoldCase(buf, capa-1, ICU_PTR(str), ICU_LEN(str), U_FOLD_CASE_DEFAULT,   &error);
    if (U_BUFFER_OVERFLOW_ERROR == error) {
        capa = len + 1;
	REALLOC_N(buf, UChar, len + 1);
	error = 0;
	len =  u_strFoldCase(buf, capa, ICU_PTR(str), ICU_LEN(str), U_FOLD_CASE_DEFAULT, &error);
    }
    return icu_ustr_new_set(buf, len, capa) ;
}

static long
icu_ustr_index(str, sub, offset)
     VALUE           str,
                     sub;
     long            offset;
{
    long            pos;
    UChar          *found;
    if (offset < 0) {
	offset += ICU_LEN(str);
	if (offset < 0)
	    return -1;
    }
    if (ICU_LEN(str) - offset < ICU_LEN(sub))
	return -1;
    if (ICU_LEN(sub) == 0)
	return offset;
    found =
	u_strFindFirst(ICU_PTR(str) + offset, ICU_LEN(str) - offset,
		       ICU_PTR(sub), ICU_LEN(sub));
    if (NULL == found)
	return -1;
    pos = found - (ICU_PTR(str) + offset);
    return pos + offset;
}

/**
 *  call-seq:
 *     str.index(substring [, offset])   => fixnum or nil
 *     str.index(regexp [, offset])      => fixnum or nil
 *  
 *  Returns the index of the first occurrence of the given <i>substring</i>,
 *  or pattern (<i>regexp</i>) in <i>str</i>. Returns
 *  <code>nil</code> if not found. If the second parameter is present, it
 *  specifies the position in the string to begin the search.
 *     
 *     "hello".u.index('e'.u)             #=> 1
 *     "hello".u.index('lo'.u)            #=> 3
 *     "hello".u.index('a'.u)             #=> nil
 *     "hello".u.index(/[aeiou]/.U, -3)   #=> 4
 */

VALUE
icu_ustr_index_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           sub;
    VALUE           initpos;
    long            pos ;
    int             processed = 0;

    if (rb_scan_args(argc, argv, "11", &sub, &initpos) == 2) {
	pos = NUM2LONG(initpos);
    } else {
	pos = 0;
    }
    if (pos < 0) {
	pos += ICU_LEN(str);
    }

    if( CLASS_OF(sub) ==  rb_cUString) {
      	pos = icu_ustr_index(str, sub, pos);
    	processed = 1;
    }
    if( CLASS_OF(sub) == rb_cURegexp) {
       	pos = icu_reg_search(sub, str, pos, 0);
    	processed = 1;
    }
    if(! processed ) {
    	rb_raise(rb_eTypeError, "Wrong Type, expected UString or URegexp, got %s", rb_class2name(CLASS_OF(sub)));
    }

    if (pos == -1)
	return Qnil;
    return LONG2NUM(pos);
}

static long
icu_ustr_rindex(str, sub, pos)
     VALUE           str,
                     sub;
     long            pos;
{
    long            len = ICU_LEN(sub);
    UChar          *found;

    /*
     * substring longer than string 
     */
    if (ICU_LEN(str) < len)
	return -1;
    if (ICU_LEN(str) - pos < len) {
	pos = ICU_LEN(str) - len;
    }
    found = u_strFindLast(ICU_PTR(str), pos, ICU_PTR(sub), ICU_LEN(sub));
    if (NULL == found)
	return -1;
    pos = found - (ICU_PTR(str));
    return pos;
}


/**
 *  call-seq:
 *     str.rindex(substring [, fixnum])   => fixnum or nil
 *     str.rindex(regexp [, fixnum])   => fixnum or nil 
 *  
 *  Returns the index of the last occurrence of the given <i>substring</i>,
 *  or pattern (<i>regexp</i>) in <i>str</i>. Returns  <code>nil</code> if not 
 *  found. If the second parameter is present, it  specifies the position in the 
 *  string to end the search---characters beyond  this point will not be considered.
 *     
 *     "hello".u.rindex('e')             #=> 1
 *     "hello".u.rindex('l')             #=> 3
 *     "hello".u.rindex('a')             #=> nil
 *     "hello".u.rindex(/[aeiou]/.U, -2)   #=> 1
 */

VALUE
icu_ustr_rindex_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           sub;
    VALUE           position;
    long            pos;

    if (rb_scan_args(argc, argv, "11", &sub, &position) == 2) {
	pos = NUM2LONG(position);
	if (pos < 0) {
	    pos += ICU_LEN(str);
	    if (pos < 0) {
		return Qnil;
	    }
	}
	if (pos > ICU_LEN(str))
	    pos = ICU_LEN(str);
    } else {
	pos = ICU_LEN(str);
    }

    switch (TYPE(sub)) {
    case T_DATA:
	if (CLASS_OF(sub) == rb_cUString) {
	    pos = icu_ustr_rindex(str, sub, pos);
	    if (pos >= 0)
		return LONG2NUM(pos);
	    break;
	}
	if (CLASS_OF(sub) == rb_cURegexp) {
	    pos = icu_reg_search(sub, str, pos, 1);
	    if (pos >= 0)
		return LONG2NUM(pos);
	    break;
	}

    default:
	rb_raise(rb_eTypeError, "type mismatch: %s given",
		 rb_obj_classname(sub));
    }
    return Qnil;
}

/**
 *  call-seq:
 *     str.lstrip!   => self or nil
 *  
 *  Removes leading whitespace from <i>str</i>, returning <code>nil</code> if no
 *  change was made. See also <code>UString#rstrip!</code> and
 *  <code>UString#strip!</code>, in all these methods whitespace is an 
 *  Unicode char that has White_Space property.
 *     
 *     "  hello  ".u.lstrip   #=> "hello  "
 *     "hello".u.lstrip!      #=> nil
 */

VALUE
icu_ustr_lstrip_bang(str)
     VALUE           str;
{
    UChar          *s;
    int32_t         i,
                    n,
                    c;
   icu_check_frozen(1, str);
    s = ICU_PTR(str);
    n = ICU_LEN(str);
    if (!s || n == 0)
	return Qnil;
    /*
     * remove spaces at head 
     */
    i = 0;
    U16_GET(s, 0, i, n, c); /* care about surrogates */
    while (i < n && u_isUWhiteSpace(c)) {
        U16_NEXT(s, i, n, c); /* care surr */
    }

    if (i > 0) {
        if(! u_isUWhiteSpace(c)) --i;
	ICU_LEN(str) = n - i;
	u_memmove(ICU_PTR(str), s + i, ICU_LEN(str));
	ICU_PTR(str)[ICU_LEN(str)] = 0;
	return str;
    }
    return Qnil;
}


/**
 *  call-seq:
 *     str.lstrip   => new_str
 *  
 *  Returns a copy of <i>str</i> with leading whitespace removed. See also
 *  <code>UString#rstrip</code> and <code>UString#strip</code>.
 *     
 *     "  hello  ".u.lstrip   #=> "hello  "
 *     "hello".u.lstrip       #=> "hello"
 */

VALUE
icu_ustr_lstrip(str)
     VALUE           str;
{
    str = icu_ustr_dup(str);
    icu_ustr_lstrip_bang(str);
    return str;
}


/**
 *  call-seq:
 *     str.rstrip!   => self or nil
 *  
 *  Removes trailing whitespace from <i>str</i>, returning <code>nil</code> if
 *  no change was made. See also <code>UString#lstrip!</code> and
 *  <code>UString#strip!</code>.
 *     
 *     "  hello  ".u.rstrip   #=> "  hello"
 *     "hello".u.rstrip!      #=> nil
 */

VALUE
icu_ustr_rstrip_bang(str)
     VALUE           str;
{
    UChar          *s;
    int32_t         i,
                    n,
                    c;

   icu_check_frozen(1, str);
    s = ICU_PTR(str);
    n = ICU_LEN(str);

    if (!s || n == 0)
	return Qnil;
    i = n - 1;

    U16_GET(s, 0, n - 1, n, c); /* care surrogates */
    i = n;
    /*
     * remove trailing spaces 
     */
    while (i > 0 && u_isUWhiteSpace(c)) {
        U16_PREV(s, 0, i, c); /* care surrogates */
    }

    if (i < n) {
	if(! u_isUWhiteSpace(c)) ++i;
	ICU_LEN(str) = i;
	ICU_PTR(str)[i] = 0;
	return str;
    }
    return Qnil;
}


/**
 *  call-seq:
 *     str.rstrip   => new_str
 *  
 *  Returns a copy of <i>str</i> with trailing whitespace removed. See also
 *  <code>UString#lstrip</code> and <code>UString#strip</code>.
 *     
 *     "  hello  ".u.rstrip   #=> "  hello"
 *     "hello".u.rstrip       #=> "hello"
 */

VALUE
icu_ustr_rstrip(str)
     VALUE           str;
{
    str = icu_ustr_dup(str);
    icu_ustr_rstrip_bang(str);
    return str;
}


/**
 *  call-seq:
 *     str.strip!   => str or nil
 *  
 *  Removes leading and trailing whitespace from <i>str</i>. Returns
 *  <code>nil</code> if <i>str</i> was not altered.
 */

VALUE
icu_ustr_strip_bang(str)
     VALUE           str;
{
    VALUE           l = icu_ustr_lstrip_bang(str);
    VALUE           r = icu_ustr_rstrip_bang(str);

    if (NIL_P(l) && NIL_P(r))
	return Qnil;
    return str;
}


/**
 *  call-seq:
 *     str.strip   => new_str
 *  
 *  Returns a copy of <i>str</i> with leading and trailing whitespace removed.
 *     
 *     "    hello    ".u.strip   #=> "hello"
 *     "\tgoodbye\r\n".u.strip   #=> "goodbye"
 */

VALUE
icu_ustr_strip(str)
     VALUE           str;
{
    str = icu_ustr_dup(str);
    icu_ustr_strip_bang(str);
    return str;
}



/* ----------------------------------- */
VALUE
icu_ustr_normalize(str, mode)
     VALUE           str;
     int32_t         mode;
{
    UErrorCode      error = U_ZERO_ERROR;
    long            capa = ICU_LEN(str)+20;
    UChar          *buf;
    long 	needed;
    VALUE ret;
    if (UNORM_YES == unorm_quickCheck(ICU_PTR(str), ICU_LEN(str), mode, &error))
	    return icu_ustr_dup(str);

    buf = ALLOC_N(UChar, capa );
    do { 
	error = 0;
	 needed =
	    unorm_normalize(ICU_PTR(str), ICU_LEN(str), mode, 0, buf, capa,
			    &error);
	if (U_SUCCESS(error)) {
	    ret = icu_ustr_new_set(buf, needed, capa);
	    return ret;
	}
	if (error == U_BUFFER_OVERFLOW_ERROR) {
	    capa = needed + 1;
	    REALLOC_N(buf, UChar, capa);
	    if (!buf)
		rb_raise(rb_eRuntimeError, "can't allocate memory");
	} else
	    rb_raise(rb_eArgError, u_errorName(error));
    }
    while (1);
}

/**
 * UNORM_NFKC Compatibility decomposition followed by canonical
 * composition. 
 */
VALUE
icu_ustr_normalize_KC(str)
     VALUE           str;
{
    return icu_ustr_normalize(str, UNORM_NFKC);
}

/**
 * UNORM_NFKD Compatibility decomposition. 
 */
VALUE
icu_ustr_normalize_KD(str)
     VALUE           str;
{
    return icu_ustr_normalize(str, UNORM_NFKD);
}

/**
 * UNORM_NFD Canonical decomposition. 
 */
VALUE
icu_ustr_normalize_D(str)
     VALUE           str;
{
    return icu_ustr_normalize(str, UNORM_NFD);
}

/**
 * UNORM_FCD 
 */
VALUE
icu_ustr_normalize_FCD(VALUE str)
{
    return icu_ustr_normalize(str, UNORM_FCD);
}

/**
 * UNORM_NFC Canonical decomposition followed by canonical composition. 
 */
VALUE
icu_ustr_normalize_C(str)
     VALUE           str;
{
    return icu_ustr_normalize(str, UNORM_NFC);
}
VALUE my_ubrk_close(UBreakIterator ** boundary, VALUE errorinfo)
{
	ubrk_close(*boundary);
	*boundary = NULL;
	rb_raise(rb_eRuntimeError, "Unhandled exception: %s", rb_obj_classname(errorinfo));
	return Qnil;
}

/* UBRK_CHARACTER, UBRK_WORD, UBRK_LINE, UBRK_SENTENCE */
VALUE
icu_ustr_each_mode(argc, argv, str, mode)
     int             argc;
     VALUE          *argv;
     VALUE           str;
     int32_t         mode;
{
    UErrorCode      error = 0;
    UBreakIterator *boundary; 
    int32_t         end, start;
    VALUE           loc ;
    VALUE           temp;
    char           *locale = "";
    if( rb_scan_args(argc, argv, "01", &loc) == 1) {
        Check_Type(loc, T_STRING);
	locale = RSTRING(loc)->ptr;
    }
    boundary =
	ubrk_open(mode, locale, ICU_PTR(str), ICU_LEN(str),
		  &error);
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, "Error %s", u_errorName(error));
     start = ubrk_first(boundary);
    ++(USTRING(str)->busy);
    for (end = ubrk_next(boundary); end != UBRK_DONE; start = end, end = ubrk_next(boundary)) {
	temp = icu_ustr_new(ICU_PTR(str) + start, end - start);
	rb_rescue(rb_yield, temp, my_ubrk_close, &boundary);
    }
    --(USTRING(str)->busy);
    ubrk_close(boundary);
    return str;
}

/**
 *  call-seq:
 *     str.each_word(locale = "")       {|substr| block }        => str
 *     
 * Word boundary analysis is used by search and replace functions, as well as within text editing 
 * applications that allow the user to select words with a double click. Word selection provides 
 * correct interpretation of punctuation marks within and following words. Characters that are not 
 * part of a word, such as symbols or punctuation marks, have word-breaks on both sides.
 *
 */
VALUE
icu_ustr_each_word(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
  
{
    return icu_ustr_each_mode(argc, argv, str,  UBRK_WORD);
}

/**
 *  call-seq:
 *     str.each_char(locale = "")       {|substr| block }        => str
 *
 * Character boundary analysis allows users to interact with characters as they expect to, 
 * for example, when moving the cursor through a text string. Character boundary analysis provides 
 * correct navigation of through character strings, regardless of how the character is stored. 
 * For example, an accented character might be stored as a base character and a diacritical mark. 
 * What users consider to be a character can differ between languages.
 *
 */
VALUE
icu_ustr_each_char(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;

{
    return icu_ustr_each_mode(argc, argv, str, UBRK_CHARACTER);
}

/**
 *  call-seq:
 *     str.each_line_break(locale = "") {|substr| block }        => str
 *
 * Line boundary analysis determines where a text string can be broken when line-wrapping. 
 * The mechanism correctly handles punctuation and hyphenated words.
 *
 */
VALUE
icu_ustr_each_line(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;

{
    return icu_ustr_each_mode(argc, argv, str,  UBRK_LINE);
}

/**
 *  call-seq:
 *     str.each_sentence(locale = "")   {|substr| block }        => str
 *
 * Sentence boundary analysis allows selection with correct interpretation of periods 
 * within numbers and abbreviations, and trailing punctuation marks such as quotation marks and parentheses.
 *
 */
VALUE
icu_ustr_each_sentence(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    return icu_ustr_each_mode(argc, argv, str,  UBRK_SENTENCE);
}

/**
 * call-seq:
 *    str.to_u(encoding = 'utf8') => UString
 *
 * Returns self.
 */
VALUE
icu_ustr_to_ustr(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    return str;
}

/**
 * call-seq:
 *    str.to_s(encoding = 'utf8') => String
 *
 * Converts to Ruby String (byte-oriented) value in  given encoding.
 * When no encoding is given, assumes UTF-8.
 */
VALUE
icu_ustr_to_rstr(argc, argv, str)
     int             argc;
     VALUE          *argv,
                     str;
{
    VALUE           enc;
    char           *encoding = 0;	/* default */
    UErrorCode      error = 0;
    UConverter     *conv ;
    int enclen, needed = 0;
    char * buf;
    VALUE s;
    if (rb_scan_args(argc, argv, "01", &enc) == 1) {
	Check_Type(enc, T_STRING);
	encoding = RSTRING(enc)->ptr;
    }
    
    enclen = ICU_LEN(str) + 1;
    buf = ALLOC_N(char, enclen);
   
    if( !encoding || !strncmp(encoding, "utf8", 4)){
	  u_strToUTF8( buf, enclen, &needed, ICU_PTR(str), ICU_LEN(str), &error);
          if (U_BUFFER_OVERFLOW_ERROR == error) {
        	REALLOC_N(buf, char, needed + 1);
        	error = 0;
        	u_strToUTF8( buf, needed, &needed, ICU_PTR(str), ICU_LEN(str), &error);
         }
	 if( U_FAILURE(error) ){
	       free(buf);
	       rb_raise(rb_eArgError, u_errorName(error));
	 }
         s = rb_str_new(buf, needed);

    } else {
            conv = ucnv_open(encoding, &error);
            if (U_FAILURE(error)) {
        	ucnv_close(conv);
		free(buf);
        	rb_raise(rb_eArgError, u_errorName(error));
            }
            enclen =
        	ucnv_fromUChars(conv, buf, enclen, ICU_PTR(str), ICU_LEN(str),
        			&error);
            if (U_BUFFER_OVERFLOW_ERROR == error) {
        	REALLOC_N(buf, char, enclen + 1);
        	error = 0;
        	ucnv_fromUChars(conv, buf, enclen, ICU_PTR(str), ICU_LEN(str),
        			&error);
            }
	    if( U_FAILURE(error) ){
	       free(buf);
	       rb_raise(rb_eArgError, u_errorName(error));
	    }
            s = rb_str_new(buf, enclen);
	    ucnv_close(conv);
    }
    free(buf);
    return s;
}

/* -------------- */
extern VALUE    icu_format(UChar * pattern, int32_t len, VALUE args,
			   int32_t arg_len, char *locale);
/**
 * call-seq:
 *     str.format(locale, [*args]) 
 *
 * Powerful locale-sensitive message formatting. see [./docs/FORMATTING]
 * 
 * Valid argument types are: +Fixnum+, +UString+, +Float+, +Time+ .
 * 
 * */
VALUE
icu_ustr_format(str, args)
     VALUE           str,
                     args;
{
    VALUE           loc;
    Check_Type(args, T_ARRAY);
    loc = rb_ary_shift(args);
    Check_Type(loc, T_STRING);
    return icu_format(ICU_PTR(str), ICU_LEN(str), args, RARRAY(args)->len,
		      RSTRING(loc)->ptr);
}

/* ------ UString regexp related functions ---- */

/**
 *  call-seq:
 *     str =~ uregexp         => UMatch or nil
 *     str =~ other_str       => integer or nil
 *  
 *  Match---If <code>URegexp</code> is given, use it as a pattern to 
 *  match against <i>uregexp</i> and return UMatch or +nil+. 
 *
 *  If <code>UString</code> is given, returns index of it  
 *  (similar to <code>UString#index</code>).
 *
 *  Otherwise returns +nil+
 *     
 *     "cat o' 9 tails".u =~ '\d'    #=> nil
 *     "cat o' 9 tails".u =~ /\d/.U  #=> #<UMatch:0xf6fb7d5c @cg=[<U000039>]>
 *     "cat o' 9 tails".u =~ 9       #=> false
 *     "cat o' 9 tails".u =~ '9'.u   #=> 7
 */

VALUE
icu_ustr_match(x, y)
     VALUE           x,
                     y;
{
    long pos ; 
    if (TYPE(y) == T_REGEXP){
      rb_raise(rb_eTypeError, "Wrong type: can't match against Regexp. Use URegexp instead");
    }
    if (CLASS_OF(y) == rb_cURegexp) {
	return icu_reg_match(y, x);
    } else if (CLASS_OF(y) == rb_cUString) {
	pos =  icu_ustr_index(x, y, 0);
	if (pos == -1) return Qnil;
	else return LONG2NUM(pos);
    } else {
	return Qnil;
    }
}

VALUE
get_pat(pat, quote)
     VALUE           pat;
     int             quote;
{
    if (CLASS_OF(pat) == rb_cURegexp)
	return pat;

    if (CLASS_OF(pat) == rb_cUString)
	return icu_reg_comp(pat);
    Check_Class(pat, rb_cURegexp);
    return Qnil;
}


/**
 *  call-seq:
 *     str.match(pattern)   => matchdata or nil
 *  
 *  Converts <i>pattern</i> to a <code>URegexp</code> (if it isn't already one),
 *  then invokes its <code>match</code> method on <i>str</i>.
 *     
 *     'hello'.u.match('(.)\1'.u)      #=> #<UMatch:0x401b3d30>
 *     'hello'.u.match('(.)\1'.u)[0]   #=> "ll"
 *     'hello'.u.match(/(.)\1/.U)[0]   #=> "ll"
 *     'hello'.u.match('xx')         #=> nil
 */

VALUE
icu_ustr_match_m(str, re)
     VALUE           str,
                     re;
{
    return rb_funcall(get_pat(re, 0), rb_intern("match"), 1, str);
}

VALUE
ustr_scan_once(str, pat, start)
     VALUE           str,
                     pat;
     long           *start;
{
    VALUE           result;
    long            i;
    long            beg,
                    end, num_regs;

    if (icu_reg_search(pat, str, *start, 0) >= 0) {
	icu_reg_range(pat, 0, &beg, &end);
	if (beg == end) {
	    *start = end + 1;
	} else {
	    *start = end;
	}
	num_regs = icu_group_count(pat);
	if (num_regs <= 1) {
	    return icu_reg_nth_match(pat, 0);
	}
	result = rb_ary_new2(num_regs);
	for (i = 1; i <= num_regs; i++) {
	    rb_ary_store(result, i - 1, icu_reg_nth_match(pat, i));
	}

	return result;
    }
    return Qnil;
}


/**
 *  call-seq:
 *     str.scan(pattern)                         => array
 *     str.scan(pattern) {|match, ...| block }   => str
 *  
 *  Both forms iterate through <i>str</i>, matching the pattern (which may be a
 *  <code>URegexp</code> or a <code>UString</code>). For each match, a result is
 *  generated and either added to the result array or passed to the block. If
 *  the pattern contains no groups, each individual result consists of the
 *  matched string.  If the pattern contains groups, each
 *  individual result is itself an array containing one entry per group.
 *     
 *     a = "cruel world".u
 *     a.scan(/\w+/.U)        #=> ["cruel", "world"]
 *     a.scan(/.../.U)        #=> ["cru", "el ", "wor"]
 *     a.scan(/(...)/.U)      #=> [["cru"], ["el "], ["wor"]]
 *     a.scan(/(..)(..)/.U)   #=> [["cr", "ue"], ["l ", "wo"]]
 *     
 *  And the block form:
 *     
 *     a.scan(/\w+/.U) {|w| print "<<#{w}>> " }
 *     print "\n"
 *     a.scan(/(.)(.)/.U) {|a,b| print b, a }
 *     print "\n"
 *     
 *  <em>produces:</em>
 *     
 *     <<cruel>> <<world>>
 *     rceu lowlr
 */

VALUE
icu_ustr_scan(str, pat)
     VALUE           str,
                     pat;
{
    VALUE           result;
    long            start = 0;

    pat = get_pat(pat, 1);
    if (!rb_block_given_p()) {
	VALUE           ary = rb_ary_new();

	while (!NIL_P(result = ustr_scan_once(str, pat, &start))) {
	    rb_ary_push(ary, result);
	}
	return ary;
    }
    ++(USTRING(str)->busy);
    while (!NIL_P(result = ustr_scan_once(str, pat, &start))) {
	rb_yield(result);
    }
    --(USTRING(str)->busy);
    return str;
}
/**
 * call-seq:
 *     str.char_span(start[, len, [locale]])
 *
 * Returns substring starting at <code>start</code>-th char, with <code>len</code> chars length.
 * Here "char" means "grapheme cluster", so start index and len are measured in terms of "graphemes"
 * locale parameter is optional.
 * Negative len can be supplied to receive to end of string.
 *
 * String is transformed to NFC before extract.
 */
VALUE
icu_ustr_char_span(int argc, VALUE * argv, VALUE str)
{
    UErrorCode      error = 0;
    int32_t         end, start, char_start = 0, char_len = -1, total_chars = 0;
    int32_t         init_pos = -1, end_pos = -1, n;
    char 	    *loc = NULL;
    VALUE 	    cs, clen, locl, out;
    UBreakIterator *boundary;

    n = rb_scan_args(argc, argv, "12", &cs, &clen, &locl);
    Check_Type(cs, T_FIXNUM);
    char_start = FIX2INT(cs);
    if(char_start < 0) rb_raise(rb_eArgError, "Negative offset aren't allowed!");
    
    if( n > 1) {
    	Check_Type(clen, T_FIXNUM);
	char_len = FIX2INT(clen);
	if(char_len <= 0) char_len = -1;
    }
    if( n > 2) {
    	Check_Type(locl, T_STRING);
	loc = RSTRING(locl)->ptr;
    }
    if(UNORM_YES != unorm_quickCheck(ICU_PTR(str), ICU_LEN(str), UNORM_NFC, &error) ) 
	    str = icu_ustr_normalize_C(str);

    boundary  =
	ubrk_open(UBRK_CHARACTER, loc, ICU_PTR(str), ICU_LEN(str), &error);
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, "Error %s", u_errorName(error));
    
    start = ubrk_first(boundary);
    for (end = ubrk_next(boundary); end != UBRK_DONE;
	 start = end, end = ubrk_next(boundary)) {
	if( total_chars == char_start ) init_pos = start;
	total_chars ++;
	if( char_len>0 && total_chars == char_start+char_len) end_pos = end;
    }
    ubrk_close(boundary);
    if( init_pos == -1) rb_raise(rb_eArgError, "Char index %d out of bounds %d", char_start, total_chars);
    if( end_pos  == -1) end_pos = ICU_LEN(str); /* reached end of string */
    out = icu_ustr_new(ICU_PTR(str)+init_pos, end_pos - init_pos);
    return out;
}

VALUE
icu_ustr_chars(str, loc)
     VALUE           str;
     char           *loc;
{
    UErrorCode      error = 0;
    int32_t         end, start;
    VALUE out;
    UBreakIterator *boundary;
    if(UNORM_YES != unorm_quickCheck(ICU_PTR(str), ICU_LEN(str), UNORM_NFC, &error) ) 
	    str = icu_ustr_normalize_C(str);

    boundary  =
	ubrk_open(UBRK_CHARACTER, loc, ICU_PTR(str), ICU_LEN(str), &error);
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, "Error %s", u_errorName(error));
    
    out = rb_ary_new();
    start = ubrk_first(boundary);
    for (end = ubrk_next(boundary); end != UBRK_DONE;
	 start = end, end = ubrk_next(boundary)) {
	rb_ary_push(out, icu_ustr_new(ICU_PTR(str) + start, end - start));
    }
    ubrk_close(boundary);
    return out;
}

/**
 * call-seq:
 *     str.chars(locale = "")  => array of character
 *
 * Returns array of character graphemes, locale dependent.
 * String is transformed to NFC before split. 
 * */
VALUE
icu_ustr_chars_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           locale;
    if (rb_scan_args(argc, argv, "01", &locale) == 1) {
	Check_Type(locale, T_STRING);
	return icu_ustr_chars(str, RSTRING(locale)->ptr);
    } else {
	return icu_ustr_chars(str, "");
    }
}

/**
 *  call-seq:
 *     str.split(pattern, [limit])   => anArray
 *  
 *  Divides <i>str</i> into substrings based on a delimiter, returning an array
 *  of these substrings. <i>str</i> is divided where the
 *  pattern matches.  
 *
 *  NOTE: split(//) or split("") is not supported. 
 *  To get array of chars use #chars or #codepoints methods
 *  
 *  If the <i>limit</i> parameter is omitted, trailing null fields are
 *  suppressed. If <i>limit</i> is a positive number, at most that number of
 *  fields will be returned (if <i>limit</i> is <code>1</code>, the entire
 *  string is returned as the only entry in an array). If negative, there is no
 *  limit to the number of fields returned, and trailing null fields are not
 *  suppressed.
 *
 *  NOTE: there's a difference in ICU regexp split and Ruby Regexp actions:
 *     "a,b,c,,".split(/,/, -1)  # => ["a", "b", "c", "", ""]
 *     "a,b,c,,".u.split(ure(","), -1)  # => ["a", "b", "c", ""]
 *  it seems to be by design, in icu/source/i18n/uregex.cpp uregex_split():
 *          if (nextOutputStringStart == inputLen) {
 *              // The delimiter was at the end of the string.  We're done.
 *              break;
 *          }
 */

VALUE
icu_ustr_split_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           spat;
    VALUE           limit = Qnil;
    int             lim = 0;
    VALUE           result;

    if (rb_scan_args(argc, argv, "11", &spat, &limit) == 2) {
	lim = NUM2INT(limit);
	if (lim <= 0)
	    limit = Qnil;
    }
    if (CLASS_OF(spat) == rb_cURegexp) {
	result = icu_reg_split(spat, str, limit);
    } else {
	if (CLASS_OF(spat) == rb_cUString) {
	    result = icu_reg_split(icu_reg_comp(spat), str, limit);
	} else {
	    rb_raise(rb_eArgError, "Expected UString or URegexp, got %s",
		     rb_class2name(CLASS_OF(spat)));
	}
    }
    if (NIL_P(limit) && lim == 0) {
	while (RARRAY(result)->len > 0 &&
	       ICU_LEN( (RARRAY(result)->ptr[RARRAY(result)->len - 1])) == 0)
	    rb_ary_pop(result);
    }

    return result;
}

/**
 * call-seq:
 *     str.inspect => String
 *
 * Shows codepoints in form of \uxxxx. For debug purposes.
 */
VALUE
icu_ustr_inspect(str)
     VALUE           str;
{
    VALUE           buf = rb_str_new2("");
    char            temp[] = "\\u0010FFFF  ";
    int32_t         i,
                    n,
		    k,
                    c;
    UChar          *s = ICU_PTR(str);
    n = ICU_LEN(str);
    i = 0;
    while (i < n) {
	U16_NEXT(s, i, n, c); /* care surrogate */
	if(c >= 0x10000) 
		k = sprintf(temp, "\\u%08X", c);
	else
		k = sprintf(temp, "\\u%04X", c);
	rb_str_cat(buf, temp, k);
    }
    return buf;
}

/**
 * call-seq:
 *     str.codepoints => array of fixnums
 *
 * Returns array of codepoints as fixnums.
 */
VALUE
icu_ustr_points(str)
     VALUE           str;
{
    VALUE           buf = rb_ary_new();
    int32_t         i,
                    n,
                    c;
    UChar          *s = ICU_PTR(str);
    n = ICU_LEN(str);
    i = 0;
    while (i < n) {
	U16_NEXT(s, i, n, c); /* care surrogates */
	rb_ary_push(buf, LONG2NUM(c));
    }
    return buf;
}


/**
 * call-seq:
 *     str.inspect_names => String
 *
 * Dumps names of codepoints in this UString (debug).
 */
VALUE
icu_ustr_inspect_names(str)
     VALUE           str;
{
    VALUE           buf = rb_str_new2("");
    char            temp[301];
    UErrorCode      error;
    int32_t         i,
                    n,
                    c,
                    l;
    UChar          *s = ICU_PTR(str);
    n = ICU_LEN(str);
    i = 0;
    while (i < n) {
	U16_NEXT(s, i, n, c) sprintf(temp, "<U%06X>", c); /* care surrogates */
	rb_str_cat(buf, temp, 9);
	error = 0;
	l = u_charName(c, U_UNICODE_CHAR_NAME, temp, 300, &error);
	rb_str_cat(buf, temp, l);
	rb_str_cat(buf, "\n", 1);
    }
    return buf;
}

VALUE
icu_ustr_subpat(str, re, nth)
     VALUE           str,
                     re;
     int             nth;
{
    if (icu_reg_search(re, str, 0, 0) >= 0) {
	return icu_reg_nth_match(re, nth);
    }
    return Qnil;
}

/* beg len are code unit indexes*/
VALUE
icu_ustr_substr(str, beg, len)
     VALUE           str;
     long            beg,
                     len;
{
    int32_t         str_size;
	str_size =  ICU_LEN(str);
	if (len < 0) return Qnil;
	
	if (beg > str_size) return Qnil;
	if (beg < 0) {
		beg += str_size;
		if (beg < 0) return Qnil;
	}
	if (beg + len > str_size) {
		len = str_size - beg;
	}
	if (len < 0) {
		len = 0;
	}
	if( len == 0) return icu_ustr_new(0, 0);
	/* adjust to codepoint boundaries */
    	U16_SET_CP_START(ICU_PTR(str), 0, beg);
	U16_SET_CP_LIMIT(ICU_PTR(str), 0, len, ICU_LEN(str));
    	return icu_ustr_new(ICU_PTR(str) + beg,  len);
}

VALUE
icu_ustr_aref(str, indx)
     VALUE           str;
     VALUE           indx;
{
    long            idx;
    int32_t         cp_len = ICU_LEN(str);

    switch (TYPE(indx)) {
    case T_FIXNUM:
	idx = FIX2LONG(indx);

      num_index:
	if (idx < 0) {
	    idx = cp_len + idx;
	}
	if (idx < 0 || cp_len <= idx) {
	    return Qnil;
	}
	return icu_ustr_substr(str, idx, 1);

    case T_DATA:
	if (CLASS_OF(indx) == rb_cURegexp)
	    return icu_ustr_subpat(str, indx, 0);
	if (CLASS_OF(indx) == rb_cUString) {
	    if (icu_ustr_index(str, indx, 0) != -1)
		return icu_ustr_dup(indx);
	    return Qnil;
	}

    default:
	/*
	 * check if indx is Range 
	 */
	{
	    long            beg,
	                    len;
	    switch (rb_range_beg_len(indx, &beg, &len, cp_len, 0)) {
	    case Qfalse:
		break;
	    case Qnil:
		return Qnil;
	    default:
		return icu_ustr_substr(str, beg, len);
	    }
	}
	idx = NUM2LONG(indx);
	goto num_index;
    }
    return Qnil;		/* not reached */
}

/**
 *  call-seq:
 *     str[fixnum]                 => new_str or nil
 *     str[fixnum, fixnum]         => new_str or nil
 *     str[range]                  => new_str or nil
 *     str[regexp]                 => new_str or nil
 *     str[regexp, fixnum]         => new_str or nil
 *     str[other_str]              => new_str or nil
 *     str.slice(fixnum)           => new_str or nil
 *     str.slice(fixnum, fixnum)   => new_str or nil
 *     str.slice(range)            => new_str or nil
 *     str.slice(regexp)           => new_str or nil
 *     str.slice(regexp, fixnum)   => new_str or nil
 *     str.slice(other_str)        => new_str or nil
 *  
 *  Element Reference---If passed a single <code>Fixnum</code>, returns 
 *  substring with the character at that position. If passed two <code>Fixnum</code>
 *  objects, returns a substring starting at the offset given by the first, and
 *  a length given by the second. If given a range, a substring containing
 *  characters at offsets given by the range is returned. In all three cases, if
 *  an offset is negative, it is counted from the end of <i>str</i>. Returns
 *  <code>nil</code> if the initial offset falls outside the string, the length
 *  is negative, or the beginning of the range is greater than the end.
 *     
 *  If a <code>URegexp</code> is supplied, the matching portion of <i>str</i> is
 *  returned. If a numeric parameter follows the regular expression, that
 *  component of the <code>UMatch</code> is returned instead. If a
 *  <code>UString</code> is given, that string is returned if it occurs in
 *  <i>str</i>. In both cases, <code>nil</code> is returned if there is no
 *  match.
 *     
 *     a = "hello there".u
 *     a[1]                   #=> 'e'
 *     a[1,3]                 #=> "ell"
 *     a[1..3]                #=> "ell"
 *     a[-3,2]                #=> "er"
 *     a[-4..-2]              #=> "her"
 *     a[12..-1]              #=> nil
 *     a[-2..-4]              #=> ""
 *     a[/[aeiou](.)\1/.U]      #=> "ell"
 *     a[/[aeiou](.)\1/.U, 0]   #=> "ell"
 *     a[/[aeiou](.)\1/.U, 1]   #=> "l"
 *     a[/[aeiou](.)\1/.U, 2]   #=> nil
 *     a["lo".u]                #=> "lo"
 *     a["bye".u]               #=> nil
 */

VALUE
icu_ustr_aref_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    if (argc == 2) {
	if (CLASS_OF(argv[0]) == rb_cURegexp) {
	    return icu_ustr_subpat(str, argv[0], NUM2INT(argv[1]));
	}
	return icu_ustr_substr(str, NUM2LONG(argv[0]), NUM2LONG(argv[1]));
    }
    if (argc != 1) {
	rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)",
		 argc);
    }
    return icu_ustr_aref(str, argv[0]);
}

/**
 *  call-seq:
 *     str.sub!(pattern, replacement)          => str or nil
 *     str.sub!(pattern) {|match| block }      => str or nil
 *  
 *  Performs the substitutions of <code>UString#sub</code> in place,
 *  returning <i>str</i>, or <code>nil</code> if no substitutions were
 *  performed.
 */

VALUE
icu_ustr_sub_bang(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
     return	ustr_gsub(argc, argv, str, 1, 1 );
}


/**
 *  call-seq:
 *     str.sub(pattern, replacement)         => new_str
 *     str.sub(pattern) {|match| block }     => new_str
 *  
 *  Returns a copy of <i>str</i> with the <em>first</em> occurrence of
 *  <i>pattern</i> replaced with either <i>replacement</i> or the value of the
 *  block. The <i>pattern</i> will typically be a <code>URegexp</code>; if it is
 *  a <code>UString</code> then no regular expression metacharacters will be
 *  interpreted (that is <code>/\d/.U</code> will match a digit, but
 *  <code>'\d'</code> will match a backslash followed by a 'd').
 *     
 *  The sequences <code>$1</code>,  <code>$2</code>, etc., may be used.
 *     
 *  In the block form, the current UMatch object is passed in as a parameter.
 *  The value returned by the block will be substituted for the match on each call.
 *     
 *     "hello".u.sub(/[aeiou]/.U, '*'.u)               #=> "h*llo"
 *     "hello".u.sub(/([aeiou])/.U, '<$1>'.u)          #=> "h<e>llo"
 */

VALUE
icu_ustr_sub(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    str = icu_ustr_dup(str);
    icu_ustr_sub_bang(argc, argv, str);
    return str;
}

/**
 * replace in string from +beg+ length +len+ (in code units)
 */
static void
icu_ustr_splice(str, beg, len, val)
     VALUE           str;
     long            beg,
                     len;
     VALUE           val;
{
    long char_len;    
    Check_Class(val, rb_cUString);
    if (val == str) {
       val = icu_ustr_dup(str);
    }
    if (len < 0)
    rb_raise(rb_eIndexError, "negative length %ld", len);
    char_len = ICU_LEN(str);

    if (char_len < beg) {
      out_of_range:
	rb_raise(rb_eIndexError, "index %ld out of string", beg);
    }
    if (beg < 0) {
	if (-beg > char_len) {
	    goto out_of_range;
	}
	beg += char_len;
    }
    if (char_len < beg + len) {
	len = char_len - beg;
    }
    	/* adjust to codepoint boundaries */
    	U16_SET_CP_START(ICU_PTR(str), 0, beg);
	U16_SET_CP_LIMIT(ICU_PTR(str), 0, len, ICU_LEN(str));

    ustr_splice_units(USTRING(str), beg, len, ICU_PTR(val), ICU_LEN(val));
    OBJ_INFECT(str, val);
}


/**
 *  call-seq:
 *     str.insert(index, other_str)   => str
 *  
 *  Inserts <i>other_str</i> before the character at the given
 *  <i>index</i>, modifying <i>str</i>. Negative indices count from the
 *  end of the string, and insert <em>after</em> the given character.
 *  The intent is insert <i>other_str</i> so that it starts at the given
 *  <i>index</i>.
 *     
 *     "abcd".u.insert(0, 'X'.u)    #=> "Xabcd"
 *     "abcd".u.insert(3, 'X'.u)    #=> "abcXd"
 *     "abcd".u.insert(4, 'X'.u)    #=> "abcdX"
 *     "abcd".u.insert(-3, 'X'.u)   #=> "abXcd"
 *     "abcd".u.insert(-1, 'X'.u)   #=> "abcdX"
 */

VALUE
icu_ustr_insert(str, idx, str2)
     VALUE           str,
                     idx,
                     str2;
{
    long            pos = NUM2LONG(idx);
    icu_check_frozen(1, str);

    if (pos == -1) {
	pos = NUM2LONG(icu_ustr_length(str));
    } else if (pos < 0) {
	pos++;
    }

    icu_ustr_splice(str, pos, 0, str2);
    return str;
}

/**
 *  call-seq:
 *     str.include? other_str   => true or false
 *  
 *  Returns <code>true</code> if <i>str</i> contains the given string 
 *     
 *     "hello".u.include? "lo".u   #=> true
 *     "hello".u.include? "ol".u   #=> false
 */

VALUE
icu_ustr_include(str, arg)
     VALUE           str,
                     arg;
{
    long            i;
    i = icu_ustr_index(str, arg, 0);
    if (i == -1)
	return Qfalse;
    return Qtrue;
}

static void
icu_ustr_subpat_set(str, re, nth, val)
     VALUE           str,
                     re;
     int             nth;
     VALUE           val;
{
    long            start,
                    end,
                    len;
    VALUE matched;

    if (icu_reg_search(re, str, 0, 0) < 0) {
	rb_raise(rb_eIndexError, "regexp not matched");
    }
    matched = icu_reg_range(re, nth, &start, &end);
    if (NIL_P(matched)) {
	rb_raise(rb_eIndexError, "regexp group %d not matched", nth);
    }
    len = end - start;
	/* adjust to codepoint boundaries */
    	U16_SET_CP_START(ICU_PTR(str), 0, start);
	U16_SET_CP_LIMIT(ICU_PTR(str), 0, len, ICU_LEN(str));

    ustr_splice_units(USTRING(str), start, len, ICU_PTR(val), ICU_LEN(val));
}

VALUE
icu_ustr_aset(str, indx, val)
     VALUE           str;
     VALUE           indx,
                     val;
{
    long            idx,
                    beg;
    long            char_len = ICU_LEN(str);

    switch (TYPE(indx)) {
    case T_FIXNUM:
      num_index:
	idx = FIX2LONG(indx);
	if (char_len <= idx) {
	  out_of_range:
	    rb_raise(rb_eIndexError, "index %ld out of string", idx);
	}
	if (idx < 0) {
	    if (-idx > char_len)
		goto out_of_range;
	    idx += char_len;
	}
	icu_ustr_splice(str, idx, 1, val);
	return val;

    case T_DATA:
	if (CLASS_OF(indx) == rb_cURegexp) {
	    icu_ustr_subpat_set(str, indx, 0, val);
	    return val;
	}
	if (CLASS_OF(indx) == rb_cUString) {
	    beg = icu_ustr_index(str, indx, 0);
	    if (beg < 0) {
		rb_raise(rb_eIndexError, "string not matched");
	    }
	    ustr_splice_units(USTRING(str), beg, ICU_LEN(indx), ICU_PTR(val), ICU_LEN(val));
	    return val;
	}
    default:
	/*
	 * check if indx is Range 
	 */
	{
	    long            beg,
	                    len;
	    if (rb_range_beg_len(indx, &beg, &len, char_len, 2)) {
		icu_ustr_splice(str, beg, len, val);
		return val;
	    }
	}
	idx = NUM2LONG(indx);
	goto num_index;
    }
}


/**
 *  call-seq:
 *     str[fixnum] = new_str
 *     str[fixnum, fixnum] = new_str
 *     str[range] = new_str
 *     str[regexp] = new_str
 *     str[regexp, fixnum] = new_str
 *     str[other_str] = new_str
 *  
 *  Element Assignment---Replaces some or all of the content of <i>str</i>. The
 *  portion of the string affected is determined using the same criteria as
 *  <code>UString#[]</code>. If the replacement string is not the same length as
 *  the text it is replacing, the string will be adjusted accordingly. If the
 *  regular expression or string is used as the index doesn't match a position
 *  in the string, <code>IndexError</code> is raised. If the regular expression
 *  form is used, the optional second <code>Fixnum</code> allows you to specify
 *  which portion of the match to replace (effectively using the
 *  <code>UMatch</code> indexing rules. The forms that take a
 *  <code>Fixnum</code> will raise an <code>IndexError</code> if the value is
 *  out of range; the <code>Range</code> form will raise a
 *  <code>RangeError</code>, and the <code>URegexp</code> and <code>UString</code>
 *  forms will silently ignore the assignment.
 */

VALUE
icu_ustr_aset_m(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    icu_check_frozen(1, str);
    if (argc == 3) {
	if (CLASS_OF(argv[0]) == rb_cURegexp) {
	    icu_ustr_subpat_set(str, argv[0], NUM2INT(argv[1]), argv[2]);
	} else {
	    icu_ustr_splice(str, NUM2LONG(argv[0]), NUM2LONG(argv[1]),
			    argv[2]);
	}
	return argv[2];
    }
    if (argc != 2) {
	rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)",
		 argc);
    }
    return icu_ustr_aset(str, argv[0], argv[1]);
}

/**
 *  call-seq:
 *     str.slice!(fixnum)           => new_str or nil
 *     str.slice!(fixnum, fixnum)   => new_str or nil
 *     str.slice!(range)            => new_str or nil
 *     str.slice!(regexp)           => new_str or nil
 *     str.slice!(other_str)        => new_str or nil
 *  
 *  Deletes the specified portion from <i>str</i>, and returns the portion
 *  deleted. The forms that take a <code>Fixnum</code> will raise an
 *  <code>IndexError</code> if the value is out of range; the <code>Range</code>
 *  form will raise a <code>RangeError</code>, and the <code>URegexp</code> and
 *  <code>UString</code> forms will silently ignore the assignment.
 *     
 *     string = "this is a string".u
 *     string.slice!(2)        #=> 105
 *     string.slice!(3..6)     #=> " is "
 *     string.slice!(/s.*t/.U)   #=> "sa st"
 *     string.slice!("r".u)      #=> "r"
 *     string                  #=> "thing"
 */

VALUE
icu_ustr_slice_bang(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    VALUE           result;
    VALUE           buf[3];
    int             i;
    icu_check_frozen(1, str);
    if (argc < 1 || 2 < argc) {
	rb_raise(rb_eArgError, "wrong number of arguments (%d for 1)",
		 argc);
    }
    for (i = 0; i < argc; i++) {
	buf[i] = argv[i];
    }
    buf[i] = icu_ustr_new(0, 0);
    result = icu_ustr_aref_m(argc, buf, str);
    if (!NIL_P(result)) {
	icu_ustr_aset_m(argc + 1, buf, str);
    }
    return result;
}

VALUE
ustr_gsub(argc, argv, str, bang, once)
     int             argc;
     VALUE          *argv;
     VALUE           str;
     int             bang;
     int	     once;
{
    VALUE           pat,
                    repl;
    long            beg,
                    end,
                    prev_end;
    int             tainted = 0,
	iter = 0;
    VALUE buf, curr_repl, umatch, block_res;
    if (argc == 1 && rb_block_given_p()) {
	iter = 1;
    } else if (argc == 2) {
	repl = argv[1];
	Check_Class(repl, rb_cUString);
	if (OBJ_TAINTED(repl))
	    tainted = 1;
    } else {
	rb_raise(rb_eArgError, "wrong number of arguments (%d for 2)",
		 argc);
    }

    pat = get_pat(argv[0], 1);
    beg = icu_reg_search(pat, str, 0, 0);

    if (beg < 0) {
	/* no match */
	if (bang)
	    return Qnil;
	return icu_ustr_dup(str);
    }
    end = 0;
//    icu_check_frozen(1, str);
    ++(USTRING(str)->busy);
    buf = icu_ustr_new(0, 0);
    pat = icu_reg_clone(pat);
    if(rb_block_given_p()) iter = 1;
    do {

	prev_end = end;
	icu_reg_range(pat, 0, &beg, &end);
	icu_ustr_concat(buf, icu_reg_get_prematch(pat, prev_end));
	if ( iter ) {
	    UChar * ptr = ICU_PTR(str);
	    long o_len  = ICU_LEN(str);
	    umatch = icu_umatch_new(pat);
	    block_res = rb_yield(umatch);
	    if (CLASS_OF(block_res) == rb_cUString)
		curr_repl = block_res;
	    else if (CLASS_OF(block_res) == rb_cUMatch)
		curr_repl = icu_umatch_aref(block_res, INT2FIX(0));
	    else
		curr_repl =
		    icu_from_rstr(0, NULL, rb_obj_as_string(block_res));
	    ustr_mod_check(str, ptr, o_len);
	} else {
	    curr_repl = icu_reg_get_replacement(pat, repl, prev_end);
	}
	icu_ustr_concat(buf, curr_repl);
    }
    while (icu_reg_find_next(pat) && !once);
    icu_ustr_concat(buf, icu_reg_get_tail(pat, end));
    --(USTRING(str)->busy);
    if (bang) {
	icu_ustr_replace(str, buf);
	return str;
    } else {
	return buf;
    }
}

/**
 *  call-seq:
 *     str.gsub!(pattern, replacement)        => str or nil
 *     str.gsub!(pattern) {|match| block }    => str or nil
 *  
 *  Performs the substitutions of <code>UString#gsub</code> in place, returning
 *  <i>str</i>, or <code>nil</code> if no substitutions were performed.
 */

VALUE
icu_ustr_gsub_bang(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    icu_check_frozen(1, str);
    return ustr_gsub(argc, argv, str, 1, 0);
}


/**
 *  call-seq:
 *     str.gsub(pattern, replacement)       => new_str
 *     str.gsub(pattern) {|match| block }   => new_str
 *  
 *  Returns a copy of <i>str</i> with <em>all</em> occurrences of <i>pattern</i>
 *  replaced with either <i>replacement</i> or the value of the block. The
 *  <i>pattern</i> will typically be a <code>URegexp</code>; if it is a
 *  <code>UString</code> then no regular expression metacharacters will be
 *  interpreted (that is <code>/\d/</code> will match a digit, but
 *  <code>'\d'</code> will match a backslash followed by a 'd').
 *     
 *  If a string is used as the replacement,  the sequences <code>$1</code>, <code>$2</code>, and so on
 *  may be used to interpolate successive groups in the match.
 *     
 *  In the block form, the current UMatch object is passed in as a parameter. The value
 *  returned by the block will be substituted for the match on each call.
 *     
 *     "hello".gsub(/[aeiou]/.U, '*')              #=> "h*ll*"
 *     "hello".gsub(/([aeiou])/.U, '<$1>')         #=> "h<e>ll<o>"
 */

VALUE
icu_ustr_gsub(argc, argv, str)
     int             argc;
     VALUE          *argv;
     VALUE           str;
{
    return ustr_gsub(argc, argv, str, 0, 0);
}


/*-------------*/


/**
 * call-seq:
 *     str.to_f( locale = "",[format_pattern]) => aFloat
 *     
 * Parses string as double value, with respect to +locale+ and format pattern,
 * if they are provided.
 *
 *      "456".u.to_f                                 # =>  456.0
 *      "123,001".u.to_f("ru")                       # =>  123.001
 *      "123,001".u.to_f("en")                       # =>  123001.0
 *      "Got 123,001".u.to_f("en", "Got ###,###".u)  # =>  123001
 */	
 
VALUE 
icu_ustr_parse_double( int argc, VALUE * argv, VALUE str)
{
	UParseError 	error;
	UErrorCode 	status = U_ZERO_ERROR;
	UNumberFormat  * format = NULL;
	VALUE		loc, pattern;
	char 		* locale;
	double		value;
	int32_t		pos, n;
	
	n =  rb_scan_args(argc, argv, "02", &loc, &pattern) ;
	if( n == 2) {
		Check_Class(pattern, rb_cUString);
	} else pattern = Qnil;
	
	if (n > 0) {
		Check_Type(loc, T_STRING);
		locale = RSTRING(loc)->ptr;
	} else locale = NULL;
	
	if( pattern != Qnil ) {
		format = unum_open(UNUM_PATTERN_DECIMAL, ICU_PTR(pattern), ICU_LEN(pattern), locale,
			&error, &status);
	} else {
		format = unum_open(UNUM_DECIMAL, NULL, 0,  locale,&error, &status);
	}
	if (U_FAILURE(status) )	rb_raise(rb_eArgError, "can't open format %s", u_errorName(status));
	pos = 0;
	value = unum_parseDouble(format, ICU_PTR(str), ICU_LEN(str), &pos, &status);
	unum_close(format);
	if (U_FAILURE(status) )	rb_raise(rb_eArgError, "can't parse %s at %d", u_errorName(status), pos);
	return rb_float_new(value);
}

/**
 * call-seq:
 *     UString::strcoll(str1, str2 )                   => Fixnum
 *     UString::strcoll(str1, str2 , locale)           => Fixnum
 *     UString::strcoll(str1, str2 , locale, strength) => Fixnum
 * 
 * Performs locale-sensitive string comparison.
 * Special values for locales can be passed in - if +nil+ is passed for the locale, 
 * the default locale collation rules will be used. If empty string ("") or "root" are 
 * passed, UCA rules will be used.
 *
 * Strength must be a fixnum that set collation strength:
 * -1 is default, 0 - primary, 1 - secondary, 2 - ternary.
 * E.g., pass 0 to ignore case and accents, 1 - to ignore case only.
 **/
VALUE
icu_ustr_coll(argc, argv, self) 
	int argc;
	VALUE *argv;
	VALUE self;
{
	UErrorCode status = 0 ;
	UCollator * collator = 0;
	int result;
	VALUE ret = Qnil;
	VALUE str1, str2, loc, strength = Qnil;
	char * locale = NULL;
	int n ;
	n = rb_scan_args(argc, argv, "22", &str1, &str2, &loc, &strength);
	if ( n == 3) {
	   if( loc != Qnil) {
		   Check_Type(loc, T_STRING);
	   	   locale = RSTRING(loc)->ptr;
           }
	}
	Check_Class(str1, rb_cUString);
	Check_Class(str2, rb_cUString);
	collator = ucol_open(locale, &status);
	if( U_FAILURE(status) )
	{
	  rb_raise(rb_eArgError, u_errorName(status));
	}
	if( n == 4 ){
	 Check_Type(strength, T_FIXNUM);
	 ucol_setStrength(collator, NUM2INT(strength));
	}
	result = ucol_strcoll(collator, ICU_PTR(str1), ICU_LEN(str1), ICU_PTR(str2), ICU_LEN(str2));
	
	switch(result){
	  case UCOL_EQUAL:   ret = INT2FIX(0);break;
	  case UCOL_GREATER: ret = INT2FIX(1);break;
	  case UCOL_LESS:    ret = INT2FIX(-1);break;
	}
	ucol_close(collator);
	return ret;
}

/**
 * call-seq:
 *     UString::list_coll => anArray
 *
 * Returns array of available collator locales, to be used in UString#strcoll
 * */
VALUE icu_ustr_list_coll(str)
	VALUE str;
{
	int32_t i, n =ucol_countAvailable();
	VALUE ret = rb_ary_new();
	for( i = 0; i<n; i++) {
	   rb_ary_push(ret, rb_str_new2(ucol_getAvailable(i)));
	}
	return ret;
}

/**
 * call-seq:
 *     UString::list_locales => anArray
 *
 * Returns array of available locales.
 * */
VALUE icu_ustr_list_locales(str)
	VALUE str;
{
	int32_t i, n =uloc_countAvailable();
	VALUE ret = rb_ary_new();
	for( i = 0; i<n; i++) {
	   rb_ary_push(ret, rb_str_new2(uloc_getAvailable(i)));
	}
	return ret;
}
/**
 * call-seq:
 *     UString::list_translits => anArray
 *
 * Returns array of available translits.
 * */
VALUE icu_ustr_list_translits(str)
	VALUE str;
{
	UErrorCode  status = U_ZERO_ERROR;
	UEnumeration *  ids ; 
	VALUE ret ;
	UChar * name;
	int32_t len;
	ids = utrans_openIDs (&status);
	ICU_RAISE(status);
	ret = rb_ary_new();
	while( (name = (UChar*)uenum_unext(ids, &len, &status))) {
		rb_ary_push(ret, icu_ustr_new(name, len));
	}
	uenum_close(ids);
	return ret;

}
/**
 * call-seq:
 *     str.search(pattern, options = {})
 *     
 * Searches for match in string. Returns array of +Range+
 * corresponding to position where pattern is matched.
 *
 * Valid options are:
 *      :locale -- locale, +String+, value e.g. "en", "ru_RU"
 *      :ignore_case --	whether to ignore case, valid values are +true+ or +false+, default to +false+
 *      :ignore_case_accents -- sets collator options to strength +0+ - primary difference, e.g. ignore case and accents, 
 *                            overrides :ignore_case: option, default to +false+, 
 *      :loosely -- same as :ignore_case_accents
 *      :limit -- Fixnum limit of match positions to return.
 *      :whole_words --  whether to match whole words only
 *      :canonical --  use canonical equivalence
 *
 *
 *     a = "A quick brown fox jumped over the lazy fox dancing foxtrote".u
 *     a.search("fox".u)                                                       # => [14..16, 39..41, 51..53]
 *     a.search("FoX".u)                                                       # => []
 *     a.search("FoX".u, :ignore_case => true)                                 # => [14..16, 39..41, 51..53]
 *     a.search("FoX".u, :ignore_case => true, :whole_words => true)           # => [14..16, 39..41]
 *     a.search("FoX".u, :ignore_case => true, :whole_words => true, :limit => 1)  # => [14..16]
 *     
 *     b = "Iñtërnâtiônàlizætiøn îs cọmpłèx".u.upcase     # => IÑTËRNÂTIÔNÀLIZÆTIØN ÎS CỌMPŁÈX
 *     b.search("nâtiôn".u, :locale => "en")                                   # => []
 *     b.search("nation".u)                                                    # => []
 *     b.search("nation".u, :locale => "en", :ignore_case_accents => true)     # => [5..10]
 *     b.search("nâtiôn".u, :locale => "en", :ignore_case => true)             # => [5..10]
 *     b.search("zaeti".u,  :locale => "en" )                                  # => []
 *     b.search("zaeti".u,  :locale => "en", :ignore_case => true)             # => []
 *     b.search("zaeti".u,  :locale => "en", :ignore_case_accents => true)     # => [14..17]
 *
 *     v = [?a, 0x0325, 0x0300].to_u # =>   ḁ̀
 *     v.search([?a, 0x300].to_u, :canonical => true) # => [0..2]
 *     v.search([?a, 0x300].to_u) # => []
 **/

VALUE icu_ustr_search(argc, argv, str)
        int           argc;
	VALUE        *argv;
	VALUE          str;
	
{
	UErrorCode status = U_ZERO_ERROR;
	UStringSearch * search = 0 ;
	VALUE  pat, locale ,  limit, options;
	int lim = -1, count = 0 ;
	int32_t start,  len;
	VALUE ret = rb_ary_new();
	UCollator * collator = 0;
	UBreakIterator * brkit = 0;
	char  * loc = 0;
        if ( rb_scan_args(argc, argv, "11", &pat, &options) == 2 ) {
	   Check_Type(options, T_HASH);
	} else {
	   options = Qnil;
	}
	
	Check_Class(pat, rb_cUString);
	locale = options == Qnil ? Qnil : rb_hash_aref(options, ID2SYM(rb_intern("locale")));
	
	if( locale != Qnil ) {
	   Check_Type(locale, T_STRING);
	   loc = RSTRING(locale) -> ptr;
	}
	limit = options == Qnil ? Qnil : rb_hash_aref(options, ID2SYM(rb_intern("limit")));
	
	if(TYPE(limit) == T_FIXNUM) {
	 lim = FIX2INT(limit);
	 if(lim <= 0) {
	   rb_raise(rb_eTypeError, "Limit must be positive or nil, got: %d", lim);
	 }
	}
	else 
  	 if (limit!=Qnil)
	    rb_raise(rb_eArgError, "Limit must be Fixnum, got %s", rb_class2name(CLASS_OF(limit)));
	    
	collator = ucol_open(loc, &status);
	ucol_setStrength(collator, -1);
	
	if( options != Qnil && Qtrue == rb_hash_aref( options, ID2SYM(rb_intern("whole_words"))) ) 
	  brkit = ubrk_open(UBRK_WORD, loc, ICU_PTR(str), ICU_LEN(str),  &status);
	  
	if( options != Qnil && Qtrue == rb_hash_aref( options, ID2SYM(rb_intern("ignore_case"))) )
	   ucol_setStrength(collator, UCOL_SECONDARY);
	   
	if( options != Qnil && 
		( Qtrue == rb_hash_aref( options, ID2SYM(rb_intern("ignore_case_accents")) ) 
		  || Qtrue == rb_hash_aref( options, ID2SYM(rb_intern("loosely")) ) 
		) 
	   )
	   ucol_setStrength(collator, UCOL_PRIMARY );
	   

	search   = usearch_openFromCollator(ICU_PTR(pat), ICU_LEN(pat), 
	                ICU_PTR(str), ICU_LEN(str), 
			collator,  brkit, &status);
			
	if( options != Qnil && Qtrue == rb_hash_aref( options, ID2SYM(rb_intern("canonical"))) )
	    usearch_setAttribute(search, USEARCH_CANONICAL_MATCH, USEARCH_ON, &status);
	
	if( U_FAILURE(status) )	   goto failure;
	
	status = U_ZERO_ERROR;
	if( usearch_first(search, &status) == USEARCH_DONE) {
	   usearch_close(search);
	   ucol_close(collator);
	   ubrk_close(brkit);
	   return ret;
	}

	do {
	  if( U_FAILURE(status) ) goto failure;
	  
	  start = usearch_getMatchedStart(search);
	  len   = usearch_getMatchedLength(search);
	  rb_ary_push(ret, rb_range_new(LONG2NUM(start), LONG2NUM(start+len-1), 0));
	  
	  status = U_ZERO_ERROR;
	  count += 1;
	  if (lim > 0 && count >= lim) break;
	} while (USEARCH_DONE != usearch_next(search, &status));
	usearch_close( search);
	ucol_close(collator);
	ubrk_close(brkit);
	return ret;

failure:
        usearch_close( search);
	ucol_close(collator);
	ubrk_close(brkit);

	rb_raise(rb_eArgError, u_errorName(status));
	return Qnil;
}
/**
 * call-seq:
 *     str.conv_unit_range(unit_range) => code_point_range
 *
 * Converts <b>code unit</b> range to <b>code point</b> range.
 * If your chars don't use multiple UTF16 codeunits, range will be the same.
 */
VALUE icu_ustr_convert_unit_range(str, range) 
	VALUE		str, range;
{
    long cu_start, cu_len, cur_pos, cp_len ;
    if( rb_range_beg_len(range, &cu_start, &cu_len, ICU_LEN(str), 0) != Qtrue)
	    return Qnil;
		    
    cur_pos  = u_countChar32( ICU_PTR(str), cu_start );
    if( cu_start+cu_len > ICU_LEN(str)) --cu_len;
    cp_len   = u_countChar32( ICU_PTR(str) + cu_start , cu_len);
    return rb_range_new(LONG2NUM(cur_pos), LONG2NUM(cur_pos + cp_len-1), 0);
}
/**
 * call-seq:
 *     str.conv_point_range(point_range) => code_unit_range
 *
 * Converts <b>code point</b> range to <b>code unit</b> range. 
 * (inversion of #conv_unit_range)
 * If your chars don't use multiple UTF16 codeuints, range will be the same.
 */
VALUE icu_ustr_convert_point_range(str, range) 
	VALUE		str, range;
{
    long cp_start,  cu_start, cu_end, cp_len, str_cp_len;
    str_cp_len = u_countChar32( ICU_PTR(str), ICU_LEN(str));
    if( Qtrue != rb_range_beg_len(range, &cp_start, &cp_len, str_cp_len, 0) )  return Qnil;
    
    cu_start = 0;
    U16_FWD_N(ICU_PTR(str), cu_start, ICU_LEN(str), cp_start); /* care sur */
    cu_end = cu_start;
    U16_FWD_N(ICU_PTR(str), cu_end, ICU_LEN(str), cp_len); /* care sur */
    
    return rb_range_new(LONG2NUM(cu_start), LONG2NUM(cu_end-1), 0);
}
/**
 * call-seq:
 *     str.unit_count
 *
 * returns number of code units in string.
 * 
 */
VALUE icu_ustr_unit_count(VALUE str){
   return LONG2NUM(ICU_LEN(str));
}
/**
 * call-seq:
 *     str.point_count
 *
 * returns number of code points in string.
 * 
 */
VALUE icu_ustr_point_count(VALUE str){
   return LONG2NUM(u_countChar32(ICU_PTR(str), ICU_LEN(str)));
}

UChar icu_uchar_at(int32_t offset, void * context) 
{
	return ((UChar*)context)[offset];
}
/**
 * call-seq:
 *    str.unescape  => new_str
 *
 * Unescape a string of characters.
 *
 * The following escape sequences are recognized:
 *     \uhhhh 4 hex digits; h in [0-9A-Fa-f] 
 *     \Uhhhhhhhh 8 hex digits 
 *     \xhh 1-2 hex digits \x{h...} 1-8 hex digits 
 *     \ooo 1-3 octal digits; o in [0-7] 
 *     \cX control-X; X is masked with 0x1F
 *     
 *  as well as the standard ANSI C escapes:
 *   \a => U+0007,  \b => U+0008, \t => U+0009, \n => U+000A, \v => U+000B, \f => U+000C, \r => U+000D, \e => U+001B, \" => U+0022, \' => U+0027, \? => U+003F, \\ => U+005C
 *
 * If escape sequence is invalid, it is ignored.
 *
 *     "\\u044D\\u043A\\u0440\\u0430\\u043D\\u0438\\u0440\\u043E\\u0432\\u0430\\u043D\\u0438\\u0435".u.unescape => "экранирование"
 *
 **/

VALUE icu_ustr_unescape(str)
	VALUE	str;
{
	UChar32 	c32;
	int32_t		offset, leng, i, segment_start;
	UChar		* ptr;
	UChar		buf[3];
	VALUE		ret;
	offset = 0;
	segment_start = 0;
	leng = ICU_LEN(str);
	ptr  = ICU_PTR(str);
	ret  = icu_ustr_new(0, 0);
	while(offset < leng) {
	    if( ptr[offset] == '\\' ) {
	    	ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, ptr+segment_start, offset-segment_start);
	    	++offset;
	 	c32 = u_unescapeAt(icu_uchar_at, &offset, leng, ICU_PTR(str));
		// append this char
		if( 0xFFFFFFFF == c32) continue;
		i = 0;
		U16_APPEND_UNSAFE(buf, i, c32);
		ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, buf, U16_LENGTH(c32));
		segment_start = offset;
	    } else {
	    	++offset;
	    }
	}
	if( segment_start < offset)
	ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, ptr+segment_start, offset-segment_start);

	return ret;
}	



/* transliteration */
extern VALUE icu_transliterate(UChar * str, int32_t str_len, UChar * id, int32_t id_len, UChar * rules, int32_t rule_len);
/**
 * call-seq:
 *     str.translit(id, [rules])
 *
 * Performs {transliteration}[http://icu.sourceforge.net/userguide/Transformations.html],
 * of this string, using given transform +id+ and +rules+
 *
 *     "yukihiro matsumoto".u.translit("Latin-Hiragana".u) # => ゆきひろ まつもと
 *     "hello".u.translit("null".u, ":: upper();".u)        # => HELLO
 **/
VALUE icu_ustr_translit(argc, argv, str)
	int argc;
	VALUE * argv ;
	VALUE str;
{
	VALUE id, rules ;
	if(rb_scan_args(argc, argv, "11", &id, &rules) == 2) {
		Check_Class(rules, rb_cUString);
	} else rules = Qnil;
	
	Check_Class(str, rb_cUString);
	Check_Class(id, rb_cUString);
	if( rules == Qnil) {
	    return icu_transliterate(ICU_PTR(str), ICU_LEN(str), ICU_PTR(id), ICU_LEN(id), NULL, 0);
	} else {
	    return icu_transliterate(ICU_PTR(str), ICU_LEN(str), ICU_PTR(id), ICU_LEN(id), 
	    			ICU_PTR(rules), ICU_LEN(rules));
	}
}
void
initialize_ustring(void)
{
    UErrorCode status = U_ZERO_ERROR;
    u_init(&status);
    if( U_FAILURE(status) ){
       rb_raise(rb_eRuntimeError, "Can't initialize : %s", u_errorName(status));
    }
    s_UCA_collator =  ucol_open("", &status);
    if( U_FAILURE(status) ){
       rb_raise(rb_eRuntimeError, "Can't initialize : %s", u_errorName(status));
    }
    s_case_UCA_collator =  ucol_open("", &status);
    if( U_FAILURE(status) ){
       rb_raise(rb_eRuntimeError, "Can't initialize : %s", u_errorName(status));
    }
    ucol_setStrength(s_case_UCA_collator, UCOL_SECONDARY);
    
/*

Document-class: UString
   
UString is a string class that stores Unicode characters directly and provides 
similar functionality as the Ruby String class.

An UString string consists of 16-bit Unicode code units. A Unicode character 
may be stored with either one code unit which is the most common case or with a matched 
pair of special code units ("surrogates"). 

For single-character handling, a Unicode character code point is a value in the 
range 0..0x10ffff. 

Indexes and offsets into and lengths of strings always count code units, not code points. 
This is the same as with multi-byte char* strings in traditional string handling. 
Operations on partial strings typically do not test for code point boundaries.

In order to use the collation, text boundary analysis, formatting and other ICU APIs, 
Unicode strings must be used. In order to get Unicode strings from your native codepage, 
you can use the conversion API.

UString class is also point for  access to several ICU services, instead of 
mirroring ICU class hierarchy.
   
====  Methods by category:
  
- concat and modify:  + ,  * ,  << ,  #concat ,  #replace  

- element reference, insert, replace:  [] ,  #slice , []= ,  #slice! ,  #insert , #char_span 

- comparisons:  <=> ,  == ,  #casecmp ,  #strcoll  

- size and positions:  #length ,  #point_count ,  #clear ,  #empty? ,  #conv_unit_range ,  #conv_point_range  

- index/search methods:  #index ,  #rindex ,  #include? ,  #search  

- regexps, matching and replacing: =~ ,  #match ,  #scan ,  #split ,  #sub ,  #sub! ,  #gsub ,  #gsub!  

- conversion String/UString:  #to_s, Kernel#u, String#to_u

- iterators:  #each_line_break ,  #each_word ,  #each_char ,  #each_sentence 

- split to chars/codepoints:  #chars ,  #codepoints , Array#to_u  

- character case:   #upcase ,  #upcase! ,  #downcase ,  #downcase!  

- stripping spaces:  #strip ,  #lstrip ,  #rstrip ,  #strip! ,  #lstrip! ,  #rstrip!  

- formatting and parsing:  #format ,  #parse_date ,  #to_f  

- UNICODE normalization:  #norm_C ,  #norm_D ,  #norm_KC ,  #norm_KD ,  #norm_FCD  

- utilities:  #unescape ,  #hash ,  #inspect ,  #inspect_names ,  #translit  

- ICU avalable info: #list_coll ,  #list_locales ,  #list_translits  
*/
    rb_cUString = rb_define_class("UString", rb_cObject);
    rb_include_module(rb_cUString, rb_mComparable);

    /* initializations */
    rb_define_alloc_func(rb_cUString, icu_ustr_alloc);
    rb_define_method(rb_cUString, "initialize", icu_ustr_init, -1);
    rb_define_method(rb_cUString, "initialize_copy", icu_ustr_replace, 1);
    rb_define_method(rb_cUString, "replace", icu_ustr_replace, 1);

    /* comparisons */
    rb_define_method(rb_cUString, "<=>", icu_ustr_cmp_m, 1);
    rb_define_method(rb_cUString, "==",  icu_ustr_equal, 1);
    rb_define_method(rb_cUString, "eql?",  icu_ustr_equal, 1);
    rb_define_method(rb_cUString, "casecmp", icu_ustr_casecmp, 1);
    rb_define_singleton_method(rb_cUString, "strcoll", icu_ustr_coll, -1);

    /* ICU avalable info */
    rb_define_singleton_method(rb_cUString, "list_coll", icu_ustr_list_coll, 0);
    rb_define_singleton_method(rb_cUString, "list_locales", icu_ustr_list_locales, 0);
    rb_define_singleton_method(rb_cUString, "list_translits", icu_ustr_list_translits, 0);

    /* hash code */
    rb_define_method(rb_cUString, "hash", icu_ustr_hash_m, 0);

    /* inspect */
    rb_define_method(rb_cUString, "inspect", icu_ustr_inspect, 0);
    rb_define_method(rb_cUString, "inspect_names", icu_ustr_inspect_names,  0);

    /* size */
    rb_define_method(rb_cUString, "length", icu_ustr_length, 0);
    rb_define_alias (rb_cUString, "size", "length");
    rb_define_method(rb_cUString, "unit_count", icu_ustr_unit_count, 0);
    rb_define_method(rb_cUString, "point_count", icu_ustr_point_count, 0);
    rb_define_method(rb_cUString, "clear", icu_ustr_clear, 0);
    rb_define_method(rb_cUString, "empty?", icu_ustr_empty, 0);

    /* UNICODE normalization */
    rb_define_method(rb_cUString, "norm_C", icu_ustr_normalize_C, 0);
    rb_define_method(rb_cUString, "norm_D", icu_ustr_normalize_D, 0);
    rb_define_method(rb_cUString, "norm_KC", icu_ustr_normalize_KC, 0);
    rb_define_method(rb_cUString, "norm_KD", icu_ustr_normalize_KD, 0);
    rb_define_method(rb_cUString, "norm_FCD", icu_ustr_normalize_FCD, 0);

    /* iterators */
    rb_define_method(rb_cUString, "each_line_break", icu_ustr_each_line, -1);
    rb_define_method(rb_cUString, "each_word", icu_ustr_each_word, -1);
    rb_define_method(rb_cUString, "each_char", icu_ustr_each_char, -1);
    rb_define_method(rb_cUString, "each_sentence", icu_ustr_each_sentence, -1);
    rb_define_alias(rb_cUString,  "each", "each_line_break");

    /* split to chars/codepoints */
    rb_define_method(rb_cUString, "chars", icu_ustr_chars_m, -1);
    rb_define_method(rb_cUString, "char_span", icu_ustr_char_span, -1);
    rb_define_method(rb_cUString, "codepoints", icu_ustr_points, 0);

    /* concat operations */
    rb_define_method(rb_cUString, "+", icu_ustr_plus, 1); 
    rb_define_method(rb_cUString, "*", icu_ustr_times, 1);
    rb_define_method(rb_cUString, "concat", icu_ustr_concat, 1);
    rb_define_alias( rb_cUString, "<<", "concat");

    /* character case  */
    rb_define_method(rb_cUString, "upcase", icu_ustr_upcase, -1);
    rb_define_method(rb_cUString, "upcase!", icu_ustr_upcase_bang, -1);
    rb_define_method(rb_cUString, "downcase", icu_ustr_downcase, -1);
    rb_define_method(rb_cUString, "downcase!", icu_ustr_downcase_bang, -1);
    rb_define_method(rb_cUString, "foldcase", icu_ustr_foldcase, 0);

    /* stripping spaces */
    rb_define_method(rb_cUString, "strip", icu_ustr_strip, 0);
    rb_define_method(rb_cUString, "lstrip", icu_ustr_lstrip, 0);
    rb_define_method(rb_cUString, "rstrip", icu_ustr_rstrip, 0);

    rb_define_method(rb_cUString, "strip!", icu_ustr_strip_bang, 0);
    rb_define_method(rb_cUString, "lstrip!", icu_ustr_lstrip_bang, 0);
    rb_define_method(rb_cUString, "rstrip!", icu_ustr_rstrip_bang, 0);

    /* index/search methods */
    rb_define_method(rb_cUString, "index", icu_ustr_index_m, -1);
    rb_define_method(rb_cUString, "rindex", icu_ustr_rindex_m, -1);
    rb_define_method(rb_cUString, "include?", icu_ustr_include, 1);
    rb_define_method(rb_cUString, "search", icu_ustr_search, -1);

    /* element reference */
    rb_define_method(rb_cUString, "[]", icu_ustr_aref_m, -1);
    rb_define_alias(rb_cUString, "slice", "[]");

    /* codeunit/codepoint conversion */
    rb_define_method(rb_cUString, "conv_unit_range", icu_ustr_convert_unit_range, 1);
    rb_define_method(rb_cUString, "conv_point_range", icu_ustr_convert_point_range, 1);

    /* insert/replace */
    rb_define_method(rb_cUString, "[]=", icu_ustr_aset_m, -1);
    rb_define_method(rb_cUString, "slice!", icu_ustr_slice_bang, -1);
    rb_define_method(rb_cUString, "insert", icu_ustr_insert, 2);

    /* conversion to String from UString */
    rb_define_method(rb_cUString, "to_u", icu_ustr_to_ustr, -1);
    rb_define_method(rb_cUString, "to_s", icu_ustr_to_rstr, -1);
    rb_define_alias(rb_cUString, "to_str", "to_s");

    /* formatting messages */
    rb_define_method(rb_cUString, "format", icu_ustr_format, -2);
    rb_define_alias( rb_cUString, "fmt", "format");

    /* parsing */
    rb_define_method(rb_cUString, "to_f", icu_ustr_parse_double, -1);

    /* transliteration */
    rb_define_method(rb_cUString, "translit", icu_ustr_translit, -1);

    /* unescaping */
    rb_define_method(rb_cUString, "unescape",  icu_ustr_unescape, 0);
    
    /* regexp matching and replacing */
    rb_define_method(rb_cUString, "=~", icu_ustr_match, 1);
    rb_define_method(rb_cUString, "match", icu_ustr_match_m, 1);
    rb_define_method(rb_cUString, "scan", icu_ustr_scan, 1);
    rb_define_method(rb_cUString, "split", icu_ustr_split_m, -1);
    rb_define_method(rb_cUString, "sub", icu_ustr_sub, -1);
    rb_define_method(rb_cUString, "sub!", icu_ustr_sub_bang, -1);
    rb_define_method(rb_cUString, "gsub", icu_ustr_gsub, -1);
    rb_define_method(rb_cUString, "gsub!", icu_ustr_gsub_bang, -1);

}

