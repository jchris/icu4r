/**
 * Document-class: UResourceBundle
 * 
 * Resource Bundle -- a collection of resource information pertaining to a given locale. 
 * A resource bundle provides a way of accessing locale-specific information in a data 
 * file. You create a resource bundle that manages the resources for a given locale and 
 * then ask it for individual resources. 
 *
 * Resource bundles can contain two main types of resources: complex and simple resources. 
 * Complex resources store other resources and can have named or unnamed elements. 
 * Tables store named elements, while arrays store unnamed ones. 
 * Simple resources contain data which can be string, binary, integer array or a single integer.
 *
 * Current ICU4R API allows only access for top-level resources by key. Found resource value is
 * translated into appropriate ruby value or structure (such as array or hash).
 *
 * See [http://icu.sourceforge.net/userguide/ResourceManagement.html] for additional info
 * about ICU resource management.
 */
#include "icu_common.h"
extern VALUE icu_ustr_new(const UChar * ptr, long len);
extern VALUE rb_cUResourceBundle;

/* --- resource bundle methods */
VALUE icu_bundle_read(UResourceBundle * r) 
{
     VALUE    value;
     UResourceBundle * temp;
     UResType res_type =  ures_getType(r);
     const UChar * u_buf;
     int32_t  len, int_val, i, n;
     const int32_t *int_ar;
     const char * buf, *key;
     UErrorCode status = U_ZERO_ERROR;
     
     switch(res_type) {
	case URES_STRING:
		u_buf = ures_getString (r, &len, &status);
		value = icu_ustr_new(u_buf, len);
		break;
		
	case URES_INT: 	
		int_val = ures_getInt (r, &status);
		value = INT2FIX(int_val);
		break;
		
	case URES_INT_VECTOR:
		int_ar =  ures_getIntVector (r, &len, &status);
		n = ures_getSize( r );
		value = rb_ary_new2(len);
		for( i = 0; i < len ; i++) {
			int_val = int_ar[i]; 
			rb_ary_store(value, i, INT2FIX(int_val));
		}
		break;

	case URES_BINARY: 	
		buf = ures_getBinary(r, &len, &status);
		value = rb_str_new( buf, len);
		break;
		
	case URES_TABLE: 
		ures_resetIterator(r );
		value = rb_hash_new();
		while(ures_hasNext(r) ){
			temp = ures_getNextResource(r, NULL,  &status);
			key = ures_getKey(temp);
			rb_hash_aset(value, rb_str_new2(key), icu_bundle_read(temp));
			ures_close(temp);
		}
	 	break;	
	case URES_ARRAY: 	
		n = ures_getSize(r);
		value = rb_ary_new2(n);
		for(i =0 ; i < n; i++) {
			temp = ures_getByIndex(r, i, NULL, &status);
			rb_ary_store(value, i, icu_bundle_read(temp));
		}     
		break;
	
     	case URES_NONE: 
	default:
		value = Qnil; break;

     }
     return value;
}

void icu_free_bundle(UResourceBundle * res) {
	ures_close(res);
}

/**
 * call-seq:
 *     UResourceBundle.open(package_name, locale)
 *
 * Opens a UResourceBundle, from which users can extract strings by using their corresponding keys. 
 *
 * Typically, +package_name+ will refer to a (.dat) file by full file or directory pathname.
 * If +nil+, ICU data will be used.
 *
 * +locale+ specifies the locale for which we want to open the resource. 
 * if +nil+, the default locale will be used. If locale == "" root locale will be used.
 *
 *       bundle = UResourceBundle.open(File.expand_path("appmsg"), "en")
 *
 */
VALUE icu_bundle_open(klass, package, locale) 
	VALUE klass, package, locale;
{

     UErrorCode 	status = U_ZERO_ERROR;
     UResourceBundle 	* r;
     
     if( package != Qnil ) Check_Type(package, T_STRING);
     if( locale  != Qnil ) Check_Type(locale,  T_STRING);
     
     r = ures_open (package == Qnil ? NULL : RSTRING(package)->ptr,
     		    locale  == Qnil ? NULL : RSTRING(locale)->ptr, &status);
     
     if (U_FAILURE(status) ) rb_raise(rb_eRuntimeError, u_errorName(status));
     return Data_Wrap_Struct(klass, 0, icu_free_bundle, r); 
}

/**
 * call-seq:
 *     UResourceBundle.open_direct(package_name, locale)
 *
 *  This function does not care what kind of localeID is passed in.
 *  It simply opens a bundle with that name. Fallback mechanism is disabled for the new bundle.
 *
 *       currency = UResourceBundle.open_direct(nil, 'CurrencyData') # open ICU currency data :)
 *       currency["CurrencyMap"]["GB"] # => GBP
 */
VALUE icu_bundle_open_direct(klass, package, locale) 
	VALUE klass, package, locale;
{

     UErrorCode 	status = U_ZERO_ERROR;
     UResourceBundle 	* r;
     
     if( package != Qnil ) Check_Type(package, T_STRING);
     if( locale  != Qnil ) Check_Type(locale, T_STRING);
     
     r = ures_openDirect (package == Qnil ? NULL : RSTRING(package)->ptr,
     		    locale  == Qnil ? NULL : RSTRING(locale)->ptr, &status);
     
     if (U_FAILURE(status) ) rb_raise(rb_eRuntimeError, u_errorName(status));
     return Data_Wrap_Struct(klass, 0, icu_free_bundle, r); 
}
/**
 * call-seq:
 *     bundle[key] => object or nil
 *
 * Returns a resource in a given bundle that has a given key or +nil+ if not found.
 *
 * Type of returned object depends on ICU type of found resource:
 *      ICU restype	description:                     returned value
 *      ------------------------------------------------------------------------------
 *	URES_STRING 	16-bit Unicode strings.          UString
 *
 *	URES_BINARY 	binary data.                     String
 *
 *	URES_TABLE 	tables of key-value pairs.       Hash
 *
 *	URES_INT 	a single 28-bit integer.         Fixnum
 *
 *	URES_ARRAY 	arrays of resources.             Array
 *
 *	URES_INT_VECTOR vectors of 32-bit integers.      Array of fixnums
 *
 */
VALUE icu_bundle_aref( argc, argv, bundle)
	int argc;
	VALUE *argv;
	VALUE bundle;
{
	UErrorCode 	status = U_ZERO_ERROR;
	UResourceBundle * res , * orig;
	VALUE 		out, tmp, locale_str;
	VALUE		key, locale_flag = Qfalse;
	int		return_locale = 0;
	if( rb_scan_args(argc, argv, "11", &key, &locale_flag) )
	{
		if(Qtrue == locale_flag) return_locale = 1;
	}
	Check_Type(key, T_STRING);
	orig = (UResourceBundle*)DATA_PTR(bundle);
	
	res = ures_getByKey(orig, RSTRING(key)->ptr, NULL, &status);
	if( U_FAILURE(status)) {
		if( status == U_MISSING_RESOURCE_ERROR)	return Qnil;
	 	rb_raise(rb_eArgError, u_errorName(status));
	}
	out = icu_bundle_read(res);
	if( return_locale ) {
		locale_str = rb_str_new2(ures_getLocale(res, &status));
		tmp = rb_ary_new();
		rb_ary_push(tmp, out);
		rb_ary_push(tmp, locale_str);
		out = tmp;
	}
	ures_close(res);
	return out;
}
/*
 * call-seq:
 *    bundle.clone
 *
 * Always throws an exception. Just a paranoia.
 */
VALUE icu_bundle_clone( bundle)
	VALUE		bundle;
{
	rb_raise(rb_eRuntimeError, "UResourceBundle can't be cloned!");
}	
void initialize_ubundle(void) { 
	rb_cUResourceBundle = rb_define_class("UResourceBundle", rb_cObject);
	rb_define_method(rb_cUResourceBundle, "[]", icu_bundle_aref, -1);
	rb_define_method(rb_cUResourceBundle, "clone", icu_bundle_clone, 0);
	rb_define_singleton_method(rb_cUResourceBundle, "open", icu_bundle_open, 2);
	rb_define_singleton_method(rb_cUResourceBundle, "open_direct", icu_bundle_open_direct, 2);
}
