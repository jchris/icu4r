#include "ruby.h"
#include <unicode/utypes.h>
#include <unicode/ustring.h>
#include <unicode/ustdio.h>
#include <unicode/uchar.h>
#include <unicode/uclean.h>
#include <unicode/uregex.h>
#include <unicode/unorm.h>
#include <unicode/ubrk.h>
#include <unicode/ucnv.h>
#include <unicode/uset.h>
#include <unicode/uenum.h>
#include <unicode/utrans.h>
#include <unicode/ucol.h>
#include <unicode/usearch.h>
#include <unicode/ures.h>
#include <unicode/unum.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
typedef struct {
    long len;
    long capa;
    UChar *ptr;
    unsigned char busy;
} ICUString ;
#define USTRING(obj) ((ICUString *)DATA_PTR(obj))
#define UREGEX(obj)  ((ICURegexp *)DATA_PTR(obj))
#define ICU_PTR(str) USTRING(str)->ptr
#define ICU_LEN(str) USTRING(str)->len
#define ICU_CAPA(str) USTRING(str)->capa
#define ICU_RESIZE(str,capacity)  REALLOC_N(ICU_PTR(str), UChar, (capacity)+1);

typedef struct  {
    URegularExpression *pattern;
    int options;
} ICURegexp;


#define  Check_Class(obj, klass)   if(CLASS_OF(obj) != klass)   rb_raise(rb_eTypeError, "Wrong type: expected %s,  got %s", rb_class2name(klass), rb_class2name(rb_obj_class(obj)));


#define ICU_RAISE(status) if(U_FAILURE(status)) rb_raise(rb_eRuntimeError, u_errorName(status));

