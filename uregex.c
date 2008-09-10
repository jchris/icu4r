
/**
 * Document-class: URegexp
 *
 * See [docs/UNICODE_REGEXPS] for details of patterns.
 *
 *      
 * Replacement Text
 *      
 * The replacement text for find-and-replace operations may contain references to 
 * capture-group text from the find. References are of the form $n, where n is the 
 * number of the capture group.
 *      
 *      Character      Descriptions
 *      $n     The text of capture group n will be substituted for $n. n must be >= 0 and not 
 *      greater than the number of capture groups. A $ not followed by a digit has no special meaning, 
 *      and will appear in the substitution text as itself, a $.
 *      \     Treat the following character as a literal, suppressing any special meaning. Backslash escaping in 
 *      substitution text is only required for '$' and '\', but may be used on any other character without bad effects.
 * 
 * 
 * Valid URegexp options are:  COMMENTS, MULTILINE, DOTALL, IGNORECASE, which can be OR'ed.
 */

#include "icu_common.h"
extern VALUE rb_cURegexp;
extern VALUE rb_cUString;
extern VALUE rb_cUMatch;
VALUE           icu_umatch_aref(VALUE match, VALUE idx);
VALUE           icu_umatch_new (VALUE re);
extern VALUE icu_ustr_new(const UChar * ptr, long len);
extern VALUE icu_ustr_new2(const UChar * ptr);
extern void ustr_splice_units(ICUString * str, long start, long del_len, const UChar * replacement, long repl_len);
extern VALUE icu_from_rstr(int, VALUE *, VALUE);

/* --------- regular expressions */
void icu_regex_free( ICURegexp      *ptr)
{
    if (ptr->pattern)
	uregex_close(ptr->pattern);
    ptr->pattern = 0;
    free(ptr);
}

VALUE
icu_reg_s_alloc(klass)
     VALUE           klass;
{
    ICURegexp      *ptr = ALLOC_N(ICURegexp, 1);
    ptr->pattern = 0;
    return Data_Wrap_Struct(klass, 0, icu_regex_free, ptr);
}

void
icu_reg_initialize(obj, s, len, options)
     VALUE           obj;
     const UChar    *s;
     long            len;
     int             options;
{
    UParseError     pe;
    UErrorCode      status = 0;
    ICURegexp      *re = UREGEX(obj);

    if (re->pattern)
	uregex_close(re->pattern);
    re->pattern = uregex_open(s, len, options, &pe, &status);
    re->options = options;

    if (U_FAILURE(status))
	rb_raise(rb_eArgError,
		 "Wrong regexp: %s line %d column %d flags %d",
		 u_errorName(status), pe.line, pe.offset, options);

}

const UChar *
icu_reg_get_pattern(ptr, len)
     ICURegexp      *ptr;
     int32_t        *len;
{
    UErrorCode      error = U_ZERO_ERROR;
    *len = 0;
    return uregex_pattern(ptr->pattern, len, &error);
}

/**
 * call-seq:
 *     URegexp.new(str [,options])
 *     URegexp.new(regexp)
 *
 *  Constructs a new regular expression from <i>pattern</i>, which can be either
 *  a <code>UString</code> or a <code>URegexp</code>.
 * */
VALUE
icu_reg_initialize_m(argc, argv, self)
     int             argc;
     VALUE          *argv;
     VALUE           self;
{
    const UChar    *s;
    int32_t         len = 0;
    int             flags = 0;

    if (argc == 0 || argc > 2) {
	rb_raise(rb_eArgError, "wrong number of arguments");
    }
    if (CLASS_OF(argv[0]) == rb_cURegexp) {
	if (argc > 1) {
	    rb_warn("flags ignored");
	}
	flags = UREGEX(argv[0])->options;
	s = icu_reg_get_pattern(UREGEX(argv[0]), &len);
    } else {
	Check_Class(argv[0], rb_cUString);
	if (argc == 2) {
	    if (FIXNUM_P(argv[1]))
		flags = FIX2INT(argv[1]);
	    else if (RTEST(argv[1]))
		flags = UREGEX_CASE_INSENSITIVE;
	}
	s = ICU_PTR(argv[0]);
	len = ICU_LEN(argv[0]);
    }
    icu_reg_initialize(self, s, len, flags);
    return self;
}

VALUE
icu_reg_new(s, len, options)
     const UChar    *s;
     long            len;
     int             options;
{
    VALUE           re = icu_reg_s_alloc(rb_cURegexp);
    icu_reg_initialize(re, s, len, options);
    return (VALUE) re;
}

VALUE 
icu_reg_clone(obj)
      VALUE       obj;
{
      ICURegexp      *regex = UREGEX(obj);
      URegularExpression *old_pattern = UREGEX(obj)->pattern;
      VALUE           ret ;
      UErrorCode      status = U_ZERO_ERROR;
      URegularExpression * new_pattern = uregex_clone(regex->pattern, &status);
       if(U_FAILURE(status) ){
        rb_raise(rb_eArgError, u_errorName(status));
      }
      ret = icu_reg_s_alloc(rb_cURegexp);
      regex = UREGEX(ret);
      regex->pattern = old_pattern;
      UREGEX(obj)->pattern = new_pattern;
      return ret;
}
VALUE
icu_reg_comp(str)
     VALUE           str;
{
    return icu_reg_new(USTRING(str)->ptr, USTRING(str)->len, 0);
}

/**
 * call-seq:
 *     regexp.to_u => URegexp
 *     
 * Converts Ruby Regexp to unicode URegexp, assuming it is in UTF8 encoding.
 * $KCODE must be set to 'u' to work reliably
 */
VALUE icu_reg_from_rb_reg(re)
      VALUE          re;
{
    return icu_reg_comp(icu_from_rstr(0, NULL, rb_funcall(re, rb_intern("to_s"), 0)));
}

/**
 * call-seq:
 *     uregex.to_u
 *
 * Returns UString of this URegexp pattern.
 * */
VALUE
icu_reg_to_u(self)
     VALUE           self;
{
    int32_t         len = 0;
    const UChar    *s = icu_reg_get_pattern(UREGEX(self), &len);
    return icu_ustr_new(s, len);
}

/**
 * call-seq:
 *     uregex.split(str, limit)
 *
 *  Divides <i>str</i> into substrings based on a regexp pattern, 
 *  returning an array of these substrings. <i>str</i> is divided where the
 *  pattern matches. 
 * */
VALUE
icu_reg_split(self, str, limit)
     VALUE           self,
                     str,
                     limit;
{
    VALUE splits;
    URegularExpression *theRegEx = UREGEX(self)->pattern;
    UErrorCode      error = U_ZERO_ERROR;
    UChar * dest_buf, **dest_fields;
    int32_t limt, req_cap, total, i;
    Check_Class(str, rb_cUString);
    if (limit != Qnil)
	Check_Type(limit, T_FIXNUM);
    dest_buf = ALLOC_N(UChar, USTRING(str)->len * 2 + 2);
    limt = (limit == Qnil ? USTRING(str)->len + 1 : FIX2INT(limit));
    dest_fields = ALLOC_N(UChar *, limt);
    uregex_setText(theRegEx, USTRING(str)->ptr, USTRING(str)->len, &error);
    if (U_FAILURE(error)) {
	free(dest_buf);
	free(dest_fields);
	rb_raise(rb_eArgError, u_errorName(error));
    }
    req_cap = 0;
    total =
	uregex_split(theRegEx, dest_buf, USTRING(str)->len * 2, &req_cap,
		     dest_fields, limt, &error);
    if (U_BUFFER_OVERFLOW_ERROR == error) {
       error = U_ZERO_ERROR;
       REALLOC_N( dest_buf, UChar, req_cap);
       total = uregex_split(theRegEx, dest_buf, req_cap, &req_cap,  dest_fields, limt, &error);
    }
    if (U_FAILURE(error) ) {
	free(dest_buf);
	free(dest_fields);
	rb_raise(rb_eArgError, u_errorName(error));
    }
    splits = rb_ary_new();
    for (i = 0; i < total; i++)
	rb_ary_push(splits, icu_ustr_new2(dest_fields[i]));
    
    	free(dest_buf);
	free(dest_fields);

    return splits;
}

long
icu_reg_search(re, str, pos, reverse)
     VALUE           re,
                     str;
     long            pos,
                     reverse;
{
    UErrorCode      error = U_ZERO_ERROR;
    long            cur_pos = 0;
    long            start,
                    last;
    
    if (!reverse) {
	start = pos;
    } else {
	start = 0;
    }

    uregex_setText(UREGEX(re)->pattern, USTRING(str)->ptr,
		   USTRING(str)->len, &error);
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, u_errorName(error));
    if (!uregex_find(UREGEX(re)->pattern, start, &error))
	return -1;
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, u_errorName(error));
    cur_pos = uregex_start(UREGEX(re)->pattern, 0, &error);
    if (reverse) {
	while (uregex_findNext(UREGEX(re)->pattern, &error)) {
	    last = uregex_start(UREGEX(re)->pattern, 0, &error);
	    error = U_ZERO_ERROR;
	    if (reverse && last > pos)
		break;
	    cur_pos = last;
	}
    }
    if (reverse && cur_pos > pos)
	return -1;
    return cur_pos;
}
long
icu_group_count(re)
     VALUE           re;
{
    UErrorCode      error = U_ZERO_ERROR;
    return uregex_groupCount(UREGEX(re)->pattern, &error);
}

VALUE
icu_reg_nth_match(re, nth)
     VALUE           re;
     long            nth;
{
    URegularExpression *the_expr = UREGEX(re)->pattern;
    UErrorCode      error = U_ZERO_ERROR;
    long            start, end;
    int32_t len;
    if( nth < 0 ) {
    	nth += icu_group_count(re) + 1;
	if(nth<=0) return Qnil;
    }
    start = uregex_start(the_expr, nth, &error);
    if (U_FAILURE(error)) {
	return Qnil;
    }
    end = uregex_end(the_expr, nth, &error);
    len = 0;
    return icu_ustr_new(uregex_getText(the_expr, &len, &error) + start,
			end - start);
}

VALUE
icu_reg_range(re, nth, start, end)
     VALUE           re;
     int             nth;
     long           *start;
     long           *end;
{
    URegularExpression *the_expr = UREGEX(re)->pattern;
    UErrorCode      error = U_ZERO_ERROR;
    if(nth < 0) {
       nth += icu_group_count(re) + 1;
       if(nth <= 0) return Qnil;
    }
    *start = uregex_start(the_expr, nth, &error);
    if (U_FAILURE(error))
	return Qnil;
    *end = uregex_end(the_expr, nth, &error);
    return Qtrue;
}

/**
 *  call-seq:
 *     uregex.match(str)   => matchdata or nil
 *     uregex =~ (str)   => matchdata or nil
 *  
 *  Returns a <code>UMatch</code> object describing the match, or
 *  <code>nil</code> if there was no match.
 *  
 *     ure("(.)(.)(.)").match("abc".u)[2]   #=> "b"
 */
VALUE
icu_reg_match(re, str)
     VALUE           re,
                     str;
{
    UErrorCode      error = U_ZERO_ERROR;
    Check_Class(str, rb_cUString);
    uregex_setText(UREGEX(re)->pattern, USTRING(str)->ptr,
		   USTRING(str)->len, &error);
    if (U_FAILURE(error))
	rb_raise(rb_eArgError, u_errorName(error));
    if (uregex_find(UREGEX(re)->pattern, 0, &error)) {
	return icu_umatch_new(re);
    }
    return Qnil;
}

/**
 *  call-seq:
 *     rxp === str   => true or false
 *  
 *  Case Equality---Synonym for <code>URegexp#=~</code> used in case statements.
 *     
 *     a = "HELLO".u
 *     case a
 *     when ure("^[a-z]*$"); print "Lower case\n"
 *     when ure("^[A-Z]*$"); print "Upper case\n"
 *     else;            print "Mixed case\n"
 *     end
 *     
 *  <em>produces:</em>
 *     
 *     Upper case
 */
VALUE
icu_reg_eqq(re, str)
     VALUE           re,
                     str;
{
    long            start;
    Check_Class(str, rb_cUString);
    start = icu_reg_search(re, str, 0, 0);
    return start < 0 ? Qfalse : Qtrue;
}




int
icu_reg_find_next(pat)
     VALUE           pat;
{
    URegularExpression *the_expr = UREGEX(pat)->pattern;
    UErrorCode      error = U_ZERO_ERROR;
    return uregex_findNext(the_expr, &error);
}

static const UChar BACKSLASH  = 0x5c;
static const UChar DOLLARSIGN = 0x24;

VALUE
icu_reg_get_replacement(pat, repl_text, prev_end)
     VALUE           pat,
                     repl_text;
     long            prev_end;
{
    UErrorCode      error = U_ZERO_ERROR;
    URegularExpression *the_expr = UREGEX(pat)->pattern;
    VALUE 	    ret = icu_ustr_new(0, 0);
    
    /* scan the replacement text, looking for substitutions ($n) and \escapes. */
    int32_t  replIdx = 0;
    int32_t  replacementLength = ICU_LEN(repl_text);
    UChar    *replacementText = ICU_PTR(repl_text);
    int32_t numDigits = 0;
    int32_t groupNum  = 0, g_start, g_end;
    UChar32 digitC;
    int32_t len;
    /* following code is rewritten version of code found  */
    /* in ICU sources : i18n/regexp.cpp */
    while (replIdx < replacementLength) {
        UChar  c = replacementText[replIdx];
        replIdx++;
        if (c != DOLLARSIGN && c != BACKSLASH) {
            /* Common case, no substitution, no escaping,  */
            /*  just copy the char to the dest buf. */
            ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, replacementText+replIdx-1, 1);
            continue;
        }

        if (c == BACKSLASH) {
            /* Backslash Escape.  Copy the following char out without further checks. */
            /*                    Note:  Surrogate pairs don't need any special handling */
            /*                           The second half wont be a '$' or a '\', and */
            /*                           will move to the dest normally on the next */
            /*                           loop iteration. */
            if (replIdx >= replacementLength) {
                break;
            }
            /* ICU4R : \uxxxx case is removed for simplicity : if (c==0x55 || c==0x75) { */

            /* Plain backslash escape.  Just put out the escaped character. */
	    ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, replacementText+replIdx, 1);
            replIdx++;
            continue;
        }

        /* We've got a $.  Pick up a capture group number if one follows. */
        /* Consume at most the number of digits necessary for the largest capture */
        /* number that is valid for this pattern. */
        numDigits = 0;
        groupNum  = 0;

        for (;;) {
            if (replIdx >= replacementLength) {
                break;
            }
            U16_GET(replacementText, 0, replIdx, replacementLength, digitC); /* care surrogates */
            if (u_isdigit(digitC) == FALSE) {
                break;
            }

            U16_FWD_1(replacementText, replIdx, replacementLength); /* care surrogates */
            groupNum=groupNum*10 + u_charDigitValue(digitC);
            numDigits++;
            if (numDigits >= 3) { /* limit 999 groups */
                break;
            }
        }

        if (numDigits == 0) {
            /* The $ didn't introduce a group number at all. */
            /* Treat it as just part of the substitution text. */
	    ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, &DOLLARSIGN, 1);
            continue;
        }

        /* Finally, append the capture group data to the destination. */
	error = U_ZERO_ERROR;
	g_start = uregex_start(the_expr, groupNum, &error);
	g_end   = uregex_end  (the_expr, groupNum, &error);
	if(U_SUCCESS(error) && g_start != -1  ) {
	   ustr_splice_units(USTRING(ret), ICU_LEN(ret), 0, 
	    uregex_getText(the_expr, &len, &error) + g_start, g_end - g_start);
	}

    }
    return ret;
}

VALUE
icu_reg_get_prematch(pat, prev_end)
     VALUE           pat;
     long            prev_end;
{
    URegularExpression *the_expr = UREGEX(pat)->pattern;
    UErrorCode      error = U_ZERO_ERROR;
    int32_t         len = 0;
    int32_t         cur_start = uregex_start(the_expr, 0, &error);
    const UChar    *temp = uregex_getText(the_expr, &len, &error);
    VALUE           pm =
	icu_ustr_new(temp + prev_end, cur_start - prev_end);
    return pm;
}

VALUE
icu_reg_get_tail(pat, prev_end)
     VALUE           pat;
     long            prev_end;
{
    UErrorCode      error = U_ZERO_ERROR;
    URegularExpression *the_expr = UREGEX(pat)->pattern;
    int32_t         len = 0;
    const UChar    *temp = uregex_getText(the_expr, &len, &error);
    VALUE           pm = icu_ustr_new(temp + prev_end, len - prev_end);
    return pm;
}

/**
 * call-seq:
 *    ure(str[, options]) => URegexp
 *
 * Creates URegexp object from UString.
 * */
VALUE
icu_reg_from_rb_str(argc, argv, obj)
     int             argc;
     VALUE          *argv;
     VALUE           obj;
{
    VALUE           pat,
                    options = Qnil;
    int             reg_opts = 0;
    if (rb_scan_args(argc, argv, "11", &pat, &options) == 1) {
	reg_opts = 0;
    } else {
	if (options != Qnil) {
	    Check_Type(options, T_FIXNUM);
	    reg_opts = FIX2INT(options);
	}
    }
    if (TYPE(pat) == T_STRING)
	pat = icu_from_rstr(0, NULL, pat);
    if (CLASS_OF(pat) != rb_cUString)
	rb_raise(rb_eArgError, "Expected String or UString");
    return icu_reg_new(ICU_PTR(pat), ICU_LEN(pat), reg_opts);
}

/**
 * call-seq:
 *     umatch[idx] => string
 *
 * Returns capture group. Group 0 is for full match.
 * */
VALUE
icu_umatch_aref(match, index)
     VALUE           match,
                     index;
{
    long idx;
    VALUE cg;
    Check_Type(index, T_FIXNUM);
    idx = FIX2LONG(index);
    cg = rb_iv_get(match, "@cg");
    return rb_ary_entry(cg, idx);
}

/**
 * call-seq:
 *     umatch.range(idx) => range
 *
 * Returns range (start, end) of capture group. Group 0 is for full match.
 *
 * NOTE: this method returns <b>code unit</b> indexes. To convert this range
 * to <b>code point</b> range use UString#conv_unit_range. If your chars don't
 * require surrogate UTF16 pairs, range will be the same. 
 * */
VALUE
icu_umatch_range(match, index)
     VALUE           match,
                     index;
{
    long idx;
    VALUE cg;
    Check_Type(index, T_FIXNUM);
    idx = FIX2LONG(index);
    cg = rb_iv_get(match, "@ranges");
    return rb_ary_entry(cg, idx);
}


/**
 * call-seq:
 *     umatch.size => fixnum
 *
 * Returns number of capture groups.
 * */
VALUE
icu_umatch_size(match)
     VALUE           match;
{
    VALUE           cg = rb_iv_get(match, "@cg");
    return LONG2NUM(RARRAY(cg)->len - 1);
}


VALUE
icu_umatch_init( self, re)
     VALUE            self, re;
{
    UErrorCode           status = U_ZERO_ERROR;
    long                 count, i, cu_start, cu_end;
    URegularExpression * the_regex;
    VALUE                obj, groups, ranges;
    
    Check_Class(re, rb_cURegexp);
    the_regex = UREGEX(re)->pattern;
    count = icu_group_count(re); 
    if (U_FAILURE(status)) {
	rb_raise(rb_eArgError, u_errorName(status));
    }
    groups = rb_ary_new2(count);
    rb_iv_set(self, "@cg", groups);
    for (i = 0; i <= count; i++) {
	obj = icu_reg_nth_match(re, i);
	rb_obj_freeze(obj);
	rb_ary_store(groups, i, obj);
    }

    ranges = rb_ary_new2(count);
    for ( i = 0; i <= count; i++){
        cu_start = uregex_start(the_regex, i, &status);
        cu_end  = uregex_end(the_regex, i, &status);
	if( cu_start == -1) rb_ary_store(ranges, i, Qnil);
	else rb_ary_store(ranges, i, rb_range_new(LONG2NUM(cu_start), LONG2NUM(cu_end-1), 0));
    }
    rb_iv_set(self, "@ranges", ranges);
    return self;
}
VALUE icu_umatch_new(re) 
	VALUE 		re;
{
	return icu_umatch_init(rb_class_new_instance(0, NULL, rb_cUMatch), re);
}




void initialize_uregexp (void) 
{
/* regular expressions */
    rb_cURegexp = rb_define_class("URegexp", rb_cObject);
    rb_define_alloc_func(rb_cURegexp, icu_reg_s_alloc);
    rb_define_method(rb_cURegexp, "initialize", icu_reg_initialize_m, -1);
    rb_define_method(rb_cURegexp, "to_u", icu_reg_to_u, 0);
    rb_define_method(rb_cURegexp, "match", icu_reg_match, 1);
    rb_define_method(rb_cURegexp, "split", icu_reg_split, 2);
    rb_define_method(rb_cURegexp, "=~", icu_reg_match, 1);
    rb_define_method(rb_cURegexp, "===", icu_reg_eqq, 1);

    /* Enable case insensitive matching.  */
    rb_define_const(rb_cURegexp, "IGNORECASE", INT2FIX(UREGEX_CASE_INSENSITIVE));
    /* Allow white space and comments within patterns   */
    rb_define_const(rb_cURegexp, "COMMENTS", INT2FIX(UREGEX_COMMENTS));
    /* Control behavior of "$" and "^" If set, recognize line terminators  within string, otherwise, match only at start and end of input  string. */
    rb_define_const(rb_cURegexp, "MULTILINE", INT2FIX(UREGEX_MULTILINE));
    /* If set, '.' matches line terminators, otherwise '.' matching stops at line end.   */
    rb_define_const(rb_cURegexp, "DOTALL", INT2FIX(UREGEX_DOTALL));


    rb_define_global_function("ure", icu_reg_from_rb_str, -1);

/**
 * Document-class: UMatch 
 *
 * Class to store information about capturing
 * groups. Used in UString#sub, UString#gsub methods, as parameter to 
 * passed block. 
 */
    rb_cUMatch = rb_define_class("UMatch", rb_cObject);
    rb_define_method(rb_cUMatch, "[]", icu_umatch_aref, 1);
    rb_define_method(rb_cUMatch, "size", icu_umatch_size, 0);
    rb_define_method(rb_cUMatch, "range", icu_umatch_range, 1);

    rb_define_method(rb_cRegexp, "to_u", icu_reg_from_rb_reg, 0);
    rb_define_alias (rb_cRegexp, "U", "to_u");
    rb_define_alias (rb_cRegexp, "ur", "to_u");

}
