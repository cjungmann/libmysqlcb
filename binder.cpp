#include <alloca.h>
#include <iostream>
#include <string.h>
#include <alloca.h>
#include <math.h>
#include <assert.h>
#include "mysqlcb.hpp"
#include "mysqlcb_binder.hpp"
using namespace std;
namespace mysqlcb {

const BD_Int<int32_t, MYSQL_TYPE_LONG>         bd_Int32("INT");
const BD_Int<uint32_t, MYSQL_TYPE_LONG, 1>     bd_UInt32("INT UNSIGNED");
const BD_Int<int16_t, MYSQL_TYPE_SHORT>        bd_Int16("SMALLINT");
const BD_Int<uint16_t, MYSQL_TYPE_SHORT, 1>    bd_UInt16("SMALLINT UNSIGNED");
const BD_Int<int8_t, MYSQL_TYPE_TINY>          bd_Int8("TINYINT");
const BD_Int<uint8_t, MYSQL_TYPE_TINY, 1>      bd_UInt8("TINYINT UNSIGNED");
const BD_Int<int64_t, MYSQL_TYPE_LONGLONG>     bd_Int64("BIGINT");
const BD_Int<uint64_t, MYSQL_TYPE_LONGLONG, 1> bd_UInt64("BIGINT UNSIGNED");

const BD_Float<double, MYSQL_TYPE_DOUBLE>      bd_Double("DOUBLE");
const BD_Float<float, MYSQL_TYPE_FLOAT>        bd_Float("FLOAT");

const BD_Date                                  bd_Date;
const BD_Time                                  bd_Time;
const BD_DateTime                              bd_DateTime("DATETIME");
const BD_TimeStamp                             bd_TimeStamp("TIMESTAMP");

const BD_String<MYSQL_TYPE_VAR_STRING>         bd_VarString("VARCHAR");
const BD_String<MYSQL_TYPE_STRING>             bd_String("CHAR");
const BD_String<MYSQL_TYPE_BLOB>               bd_Blob("BLOB");

const BD_String<MYSQL_TYPE_ENUM>               bd_Enum("ENUM");
const BD_String<MYSQL_TYPE_SET>                bd_Set("SET");

MParam::MParam(const char *str)
   : m_size(strlen(str)), m_data(str), m_type(&bd_VarString) { }

MParam::MParam(int &val)
   : m_size(sizeof(int)), m_data(&val), m_type(&bd_Int32) { }

MParam::MParam(unsigned int &val)
   : m_size(sizeof(unsigned int)), m_data(&val), m_type(&bd_UInt32) { }

MParam::MParam(double &val)
   : m_size(sizeof(double)), m_data(&val), m_type(&bd_Double) { }



const BDType *typerefs[] = {
   &bd_Int32,
   &bd_UInt32,
   &bd_Int16,
   &bd_UInt16,
   &bd_Int8,
   &bd_UInt8,
   &bd_Int64,
   &bd_UInt64,
   &bd_Double,
   &bd_Float,
   &bd_Date,
   &bd_Time,
   &bd_DateTime,
   &bd_TimeStamp,
   &bd_VarString,
   &bd_String,
   &bd_Blob,
   &bd_Enum,
   &bd_Set,

   nullptr
};

const BDType *get_bdtype(const MYSQL_FIELD &fld)
{
   enum_field_types ftype = fld.type;
   const BDType **ptr = typerefs;
   while(*ptr)
   {
      if ((*ptr)->field_type()==ftype)
         return *ptr;
      ++ptr;
   };
   
   return nullptr;
}

const BDType *get_bdtype(const char *name)
{
   const BDType **ptr = typerefs;
   while(*ptr)
   {
      if ((*ptr)->name_match(name))
         return *ptr;
      ++ptr;
   };
   
   return nullptr;
}

bool confirm_bdtype(const BDType *t)
{
   const BDType **ptr = typerefs;
   while(*ptr)
   {
      if (*ptr == t)
         return true;
      ++ptr;
   };
   return false;
}


uint32_t get_buffer_size(const MYSQL_FIELD &fld)
{
   switch (fld.type)
   {
      case MYSQL_TYPE_TINY:      return sizeof(int8_t);
      case MYSQL_TYPE_SHORT:     return sizeof(int16_t);
      case MYSQL_TYPE_LONG:      return sizeof(int32_t);
      case MYSQL_TYPE_LONGLONG:  return sizeof(int64_t);
      case MYSQL_TYPE_FLOAT:     return sizeof(float);
      case MYSQL_TYPE_DOUBLE:    return sizeof(double);

      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_TIMESTAMP: return sizeof(MYSQL_TIME);

      default:
         return fld.length;
   }
}

/**
 * @brief Attaches addresses to pointers of MYSQL_BIND struct.
 *
 * Note that this must be done before set_bind_values_from_field() or
 * the pointers that function uses will be invalid.
 */
void set_bind_pointers_to_data_members(MYSQL_BIND &b, Bind_Data &d)
{
   b.length = &d.len_data;
   b.is_null = &d.is_null;
   b.error = &d.is_error;
}

void set_bind_values_from_field(MYSQL_BIND &b, const MYSQL_FIELD &f)
{
   // set_bind_pointers_to_data_members() must be called first.
   assert(b.length);

   b.buffer_type = f.type;
   b.is_unsigned = (f.flags & UNSIGNED_FLAG)!=0;
   *b.length = get_buffer_size(f);
}

void set_bind_values(MYSQL_BIND &b, const BaseParam &param)
{
   // set_bind_pointers_to_data_members() must be called first.
   assert(b.length);

   b.buffer_type = param.field_type();
   b.is_unsigned = param.is_unsigned();
   *(b.length) = param.get_data_len();
}

void set_bind_values(MYSQL_BIND &b, const MParam *param)
{
   // set_bind_pointers_to_data_members() must be called first.
   assert(b.length);

   b.buffer_type = param->field_type();
   b.is_unsigned = param->is_unsigned();
   *(b.length) = param->size();
}

void set_bind_data_object_pointers(Bind_Data &bd, MYSQL_FIELD &field, MYSQL_BIND &bind)
{
   bd.field = &field;
   bd.bind = &bind;
   bd.bdtype = get_bdtype(field);
}

void get_result_binds(MYSQL &mysql, IBinder_Callback &cb, MYSQL_STMT *stmt)
{
   uint32_t num_fields = mysql_stmt_field_count(stmt);
   if (num_fields)
   {
      MYSQL_RES *result = mysql_stmt_result_metadata(stmt);
      if (result)
      {
         MYSQL_FIELD *fields = mysql_fetch_fields(result);
         MYSQL_BIND  *binds = static_cast<MYSQL_BIND*>(alloca(sizeof(MYSQL_BIND) * num_fields));
         // Make one extra Bind_Data element, set to NULL, to signal the end of the list.
         Bind_Data   *bdata = static_cast<Bind_Data*>(alloca(sizeof(Bind_Data) * (num_fields+1)));

         memset(binds, 0, sizeof(MYSQL_BIND)*num_fields);
         memset(bdata, 0, sizeof(Bind_Data)*(num_fields+1));

         for (uint32_t i=0; i<num_fields; ++i)
         {
            Bind_Data  &bdataInst = bdata[i];
            MYSQL_FIELD &field = fields[i];
            MYSQL_BIND &bind = binds[i];

            set_bind_pointers_to_data_members(bind, bdataInst);
            set_bind_values_from_field(bind, field);
            set_bind_data_object_pointers(bdataInst, field, bind);

            if (is_unsupported_type(bdataInst))
            {
               static const char *unp = " unprepared field type.";
               static const int len_unp = strlen(unp);

               int len = strlen(field.name);
               char *buff = static_cast<char*>(alloca(len+len_unp+1));
               char *ptr = buff;
               memcpy(ptr, field.name, len);
               ptr += len;
               memcpy(ptr, unp, len_unp);
               ptr[len_unp] = '\0';
               throw std::runtime_error(ptr);
            }

            // uint32_t buffer_length = get_buffer_size(fields[i]);
            uint32_t buffer_length = required_buffer_size(bdataInst);

            if (buffer_length>1024)
               buffer_length = 1024;

            // alloca must be in this scope to persist until callback:
            bind.buffer = bdata[i].data = static_cast<void*>(alloca(buffer_length));
            bind.buffer_length = buffer_length;
         }

         Binder b = { num_fields, fields, binds, bdata };
         cb(b);

         mysql_free_result(result);
      }
      else
      {
         throw std::runtime_error("Error getting result metadata.");
      }
   }
}

void summon_binder(IBinder_Callback &cb, ...)
{
   va_list args;
   va_start(args, cb);

   va_list counter;
   va_copy(counter, args);
   int num_params = 0;
   while(1)
   {
      const BaseParam &bp = va_arg(counter, BaseParam);
      if (bp.is_void())
         break;
      else
         ++num_params;
   }
   va_end(counter);

   size_t memlen = num_params * sizeof(MYSQL_BIND);
   MYSQL_BIND *binds = static_cast<MYSQL_BIND*>(alloca(memlen));
   memset(static_cast<void*>(binds), 0, memlen);

   memlen = (num_params+1) * sizeof(Bind_Data);
   Bind_Data *bind_data = static_cast<Bind_Data*>(alloca(memlen));
   memset(static_cast<void*>(bind_data), 0, memlen);

   Binder binder = { static_cast<uint32_t>(num_params), nullptr, binds, bind_data };
   
   MYSQL_BIND *p_bind = binds;
   Bind_Data *p_data = bind_data;
   while(1)
   {
      const BaseParam &param = va_arg(args, BaseParam);
      if (param.is_void())
         break;
      else
      {
         set_bind_pointers_to_data_members(*p_bind, *p_data);
         set_bind_values(*p_bind, param);
         p_data->bind = p_bind;

         // uint32_t buffer_length = get_buffer_size(fields[i]);
         uint32_t buffer_length = param.get_data_len();

         // alloca must be in this scope to persist until callback:
         void *data = alloca(buffer_length);
         memcpy(data, param.data(), buffer_length);
         p_bind->buffer = p_data->data = data;
         p_bind->buffer_length = buffer_length;

         ++p_bind;
         ++p_data;
      }
   }
   va_end(args);

   cb(binder);
}


void summon_binder(IBinder_Callback &cb, const MParam *params)
{
   const MParam *ptr = nullptr;

   // Count params ahead of allocating pointer arrays:
   uint32_t num_params = 0;
   ptr = params;
   while (ptr++->is_valid())
      ++num_params;

   // Allocate arrays memory and install into a binder object:
   size_t memlen = num_params * sizeof(MYSQL_BIND);
   MYSQL_BIND *binds = static_cast<MYSQL_BIND*>(alloca(memlen));
   memset(static_cast<void*>(binds), 0, memlen);

   memlen = (num_params+1) * sizeof(Bind_Data);
   Bind_Data *bind_data = static_cast<Bind_Data*>(alloca(memlen));
   memset(static_cast<void*>(bind_data), 0, memlen);

   Binder binder = {num_params, nullptr, binds, bind_data };
   
   MYSQL_BIND *p_bind = binds;
   Bind_Data *p_data = bind_data;


   ptr = params;
   while (ptr->is_valid())
   {
      set_bind_pointers_to_data_members(*p_bind, *p_data);
      set_bind_values(*p_bind, ptr);
      p_data->bind = p_bind;

      // uint32_t buffer_length = get_buffer_size(fields[i]);
      uint32_t buffer_length = ptr->size();

      // alloca must be in this scope to persist until callback:
      void *data = alloca(buffer_length);
      memcpy(data, ptr->data(), buffer_length);
      p_bind->buffer = p_data->data = data;
      p_bind->buffer_length = buffer_length;

      ++p_bind;
      ++p_data;

      ++ptr;
   }

   cb(binder);
}

}// namespace
