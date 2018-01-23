#ifndef MYSQLCB_BINDER_HPP_SOURCE
#define MYSQLCB_BINDER_HPP_SOURCE

#include <mysql.h>
#include <stdint.h>  // for uint32_t
#include <iostream>

struct Bind_Data;

class BDType
{
public:
   virtual ~BDType() {}
   virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const = 0;
   virtual size_t get_size(const Bind_Data &bd) const = 0;
   virtual void set_with_value(void* buff, size_t len, const Bind_Data &bd) const = 0;
};

/**
 * This struct is alloced to provide targets for MYSQL_BIND pointer members.
 */
struct Bind_Data
{
   // The first four members will be mapped into the MYSQL_BIND structure:
   unsigned long len_data;
   my_bool       is_null;
   void          *data;
   my_bool       is_error;

   // The final member is true for long strings that must be paged out:
   bool          is_truncated;
   const BDType  *bdtype;

   
   size_t get_size(void)                            { return bdtype->get_size(*this); }
   void set_with_value(void *buff, size_t len_buff) { bdtype->set_with_value(buff,len_buff,*this); }
};


inline std::ostream& operator<<(std::ostream &os, const Bind_Data &obj)
{
   return obj.bdtype->stream_it(os, obj);
}


/**
 * Bundle of Field information, collected from mysql_stmt_result_metadata.
 *
 * This structure is passed to a callback function each time a row is fetched
 * from a query.
 *
 * The associated arrays allows access to 
 */
struct Binder
{
   uint32_t    field_count;
   MYSQL_FIELD *fields;
   MYSQL_BIND  *binds;
   Bind_Data   *bind_data;
};


class IBinder_Callback
{
public:
   virtual ~IBinder_Callback() {}
   virtual void operator()(Binder &b) const=0;
};

template <typename Func>
class Binder_User : public IBinder_Callback
{
protected:
   const Func &m_f;
public:
   Binder_User(const Func &f) : m_f(f)      {}
   virtual ~Binder_User()                   {}
   virtual void operator()(Binder &b) const { m_f(b); }
};

uint32_t get_bind_size(MYSQL_FIELD *fld);
void get_result_binds(MYSQL &mysql, IBinder_Callback &cb, MYSQL_STMT *stmt);

template <typename T>
class BD_Num : public BDType
{
public:
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      os << *static_cast<T*>(bd.data);
      return os;
   }
   inline virtual size_t get_size(const Bind_Data &bd) const
   {
      return sizeof(T);
   }
   inline virtual void set_with_value(void* buff, size_t len, const Bind_Data &bd) const
   {
      memcpy(buff, bd.data, sizeof(T));
   }
};

using BD_Int32 = BD_Num<int32_t>;
using BD_UInt32 = BD_Num<uint32_t>;
using BD_Int16 = BD_Num<int16_t>;
using BD_UInt16 = BD_Num<uint16_t>;
using BD_Int8 = BD_Num<int8_t>;
using BD_UInt8 = BD_Num<uint8_t>;
using BD_Int64 = BD_Num<int64_t>;
using BD_UInt64 = BD_Num<uint64_t>;

using BD_Double = BD_Num<double>;
using BD_Float = BD_Num<float>;


class BD_String : public BDType
{
public:
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      os.write(static_cast<const char*>(bd.data), bd.len_data);
      return os;
   }

   inline virtual size_t get_size(const Bind_Data &bd) const
   {
      return bd.len_data+1;
   }
   inline virtual void set_with_value(void* buff, size_t len, const Bind_Data &bd) const
   {
      memcpy(buff, bd.data, bd.len_data);
      if (len > bd.len_data)
         static_cast<char*>(buff)[bd.len_data] = '\0';
   }
   
};





#endif

