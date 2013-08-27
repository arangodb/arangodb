/*
** mruby/data.h - Data class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_DATA_H
#define MRUBY_DATA_H 1

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct mrb_data_type {
  const char *struct_name;
  void (*dfree)(mrb_state *mrb, void*);
} mrb_data_type;

struct RData {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  const mrb_data_type *type;
  void *data;
};

struct RData *mrb_data_object_alloc(mrb_state *mrb, struct RClass* klass, void *datap, const mrb_data_type *type);

#define Data_Wrap_Struct(mrb,klass,type,ptr)\
  mrb_data_object_alloc(mrb,klass,ptr,type)

#define Data_Make_Struct(mrb,klass,strct,type,sval,data) do { \
  sval = mrb_malloc(mrb, sizeof(strct));                     \
  { static const strct zero = { 0 }; *sval = zero; };\
  data = Data_Wrap_Struct(mrb,klass,type,sval);\
} while (0)

#define RDATA(obj)         ((struct RData *)(mrb_ptr(obj)))
#define DATA_PTR(d)        (RDATA(d)->data)
#define DATA_TYPE(d)       (RDATA(d)->type)
void mrb_data_check_type(mrb_state *mrb, mrb_value, const mrb_data_type*);
void *mrb_data_get_ptr(mrb_state *mrb, mrb_value, const mrb_data_type*);
#define DATA_GET_PTR(mrb,obj,dtype,type) (type*)mrb_data_get_ptr(mrb,obj,dtype)
void *mrb_data_check_get_ptr(mrb_state *mrb, mrb_value, const mrb_data_type*);
#define DATA_CHECK_GET_PTR(mrb,obj,dtype,type) (type*)mrb_data_check_get_ptr(mrb,obj,dtype)

/* obsolete functions and macros */
#define mrb_data_check_and_get(mrb,obj,dtype) mrb_data_get_ptr(mrb,obj,dtype)
#define mrb_get_datatype(mrb,val,type) mrb_data_get_ptr(mrb, val, type)
#define mrb_check_datatype(mrb,val,type) mrb_data_get_ptr(mrb, val, type)
#define Data_Get_Struct(mrb,obj,type,sval) do {\
  *(void**)&sval = mrb_data_get_ptr(mrb, obj, type); \
} while (0)

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif /* MRUBY_DATA_H */
