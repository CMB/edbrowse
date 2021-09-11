/^struct JSRuntime {$/,$!d
1,/list_head job_list;/!d
s/JSRuntime/modifiedRuntime/
/^ *\/\*.*\*\/$/d
/List of allocated GC objects/d
/by the garbage collector/d
/list_head string_list/d
/^ *$/d
s/;.*/;/
s/.*\*/    void */
s/JSGCPhaseEnum/uchar/
s/BOOL/uchar/
s/list_head job_list;/&};/
