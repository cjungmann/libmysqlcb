#include <alloca.h>
#include <iostream>
#include <string.h>
#include <alloca.h>
#include <assert.h>
#include "mysqlcb.hpp"
#include "mysqlcb_binder.hpp"
using namespace std;

const BD_Num<int32_t, MYSQL_TYPE_LONG> bd_Int32("INT");
const BD_Num<uint32_t, MYSQL_TYPE_LONG, 1> bd_UInt32("INT UNSIGNED");
const BD_Num<int16_t, MYSQL_TYPE_SHORT> bd_Int16("SHORT");
const BD_Num<uint16_t, MYSQL_TYPE_SHORT, 1> bd_UInt16("SHORT UNSIGNED");
const BD_Num<int8_t, MYSQL_TYPE_TINY> bd_Int8("TINY");
const BD_Num<uint8_t, MYSQL_TYPE_TINY, 1> bd_UInt8("TINY UNSIGNED");
const BD_Num<int64_t, MYSQL_TYPE_LONGLONG> bd_Int64("LONG LONG");
const BD_Num<uint64_t, MYSQL_TYPE_LONGLONG, 1> bd_UInt64("LONG LONG UNSIGNED");

const BD_Num<double, MYSQL_TYPE_DOUBLE> bd_Double("DOUBLE");
const BD_Num<float, MYSQL_TYPE_FLOAT> bd_Float("FLOAT");

const BD_String<MYSQL_TYPE_VAR_STRING> bd_VarString("VARCHAR");
const BD_String<MYSQL_TYPE_STRING> bd_String("CHAR");
const BD_String<MYSQL_TYPE_BLOB> bd_Blob("BLOB");


const BDType *typerefs[] = {
   &bd_Int32,
   &bd_UInt32,
   &bd_Int16,
   &bd_UInt16,
   &bd_Int8,
   &bd_UInt8,
   &bd_Int64,
   &bd_UInt64,
   &bd_VarString,
   &bd_String,
   &bd_Blob,

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

void set_bind_data_object_pointers(Bind_Data &bd, MYSQL_FIELD &field, MYSQL_BIND &bind)
{
   bd.field = &field;
   bd.bind = &bind;
   bd.bdtype = get_bdtype(field);
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
         // Make one extra Bind_Data element, set to NULL, to signal the end of the list.
         Bind_Data   *bdata = static_cast<Bind_Data*>(alloca(sizeof(Bind_Data) * (fcount+1)));

         memset(binds, 0, sizeof(MYSQL_BIND)*fcount);
         memset(bdata, 0, sizeof(Bind_Data)*(fcount+1));

         for (uint32_t i=0; i<fcount; ++i)
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
