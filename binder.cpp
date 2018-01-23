#include <alloca.h>
#include <iostream>
#include <string.h>
#include <alloca.h>
#include "mysqlcb.hpp"
#include "mysqlcb_binder.hpp"
using namespace std;

const BD_Num<int32_t> g_bd_int32;
const BD_Num<uint32_t> g_bd_uint32;

const BD_String  g_bd_string;


const BDType *get_bdtype(const MYSQL_FIELD &fld)
{
   bool is_unsigned = (fld.flags & UNSIGNED_FLAG) !=0;
   switch(fld.type)
   {
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_LONG:
         if (is_unsigned)
            return &g_bd_uint32;
         else
            return &g_bd_int32;
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_YEAR:
      case MYSQL_TYPE_NEWDATE:
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
         return &g_bd_string;
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
         break;
   }

   return nullptr;
}


void set_bind_pointers_to_data_members(MYSQL_BIND &b, Bind_Data &d)
{
   b.length = &d.len_data;
   b.is_null = &d.is_null;
   b.error = &d.is_error;
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

void set_bind_values_from_field(MYSQL_BIND &b, const MYSQL_FIELD &f)
{
   b.buffer_type = f.type;
   b.is_unsigned = (f.flags & UNSIGNED_FLAG)!=0;
   *b.length = get_buffer_size(f);
}

void get_result_binds(MYSQL &mysql, IBinder_Callback &cb, MYSQL_STMT *stmt)
{
   uint32_t fcount = mysql_stmt_field_count(stmt);
   if (fcount)
   {
      MYSQL_RES *result = mysql_stmt_result_metadata(stmt);
      if (result)
      {
         MYSQL_FIELD *fields = mysql_fetch_fields(result);
         MYSQL_BIND  *binds = static_cast<MYSQL_BIND*>(alloca(sizeof(MYSQL_BIND) * fcount));
         Bind_Data   *bdata = static_cast<Bind_Data*>(alloca(sizeof(Bind_Data) * fcount));

         memset(binds, 0, sizeof(MYSQL_BIND)*fcount);
         memset(bdata, 0, sizeof(Bind_Data)*fcount);

         for (uint32_t i=0; i<fcount; ++i)
         {
            MYSQL_FIELD &field = fields[i];
            MYSQL_BIND &bind = binds[i];
            Bind_Data  &bdataInst = bdata[i];
            set_bind_pointers_to_data_members(bind, bdataInst);
            set_bind_values_from_field(bind, field);
            bdataInst.bdtype = get_bdtype(field);

            uint32_t buffer_length = get_buffer_size(fields[i]);

            if (buffer_length>1024)
               buffer_length = 2014;

            // alloca must be in this scope to persist until callback:
            bind.buffer = bdata[i].data = static_cast<void*>(alloca(buffer_length));
            bind.buffer_length = buffer_length;
         }

         Binder b = { fcount, fields, binds, bdata };
         cb(b);

         mysql_free_result(result);
      }
      else
      {
         cerr << "Error getting result metadata." << endl;
      }
   }
}
