extern void initialize_ustring(void);
extern void initialize_calendar(void);
extern void initialize_uregexp(void);
extern void initialize_ucore_ext(void);
extern void initialize_ubundle(void);
extern void initialize_converter(void);
extern void initialize_collator(void);
void Init_icu4r (void) {

 initialize_ustring();
 initialize_uregexp();
 initialize_ucore_ext();
 initialize_ubundle();
 initialize_calendar();
 initialize_converter();
 initialize_collator();

}
