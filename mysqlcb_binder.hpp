#ifndef MYSQLCB_BINDER_HPP_SOURCE
#define MYSQLCB_BINDER_HPP_SOURCE

#include <mysql.h>
#include <stdint.h>  // for uint32_t
#include <string.h>  // for memcpy()
#include <iostream>

/**
 * @brief Simple interface for callback.
 * @sa Generic_User
 * @sa @ref Templated_Callbacks
 */
template <class T>
class IGeneric_Callback
{
public:
   virtual ~IGeneric_Callback() { }
   virtual void operator()(T &t) const = 0;
};

/**
 * @brief Generic User class for callbacks with constructed objects.
 *
 * @tparam T      Class of the object that will be constructed.
 * @tparam Func   Function object that takes a reference to a class C object.
 *
 * @sa IGeneric_Callback
 * @sa @ref Templated_Callbacks
 */
template <class T, class Func>
class Generic_User : public IGeneric_Callback<T>
{
protected:
   const Func &m_f;
public:
   Generic_User(const Func &f) : m_f(f) { }
   virtual ~Generic_User()              { }
   virtual void operator()(T &t) const  { m_f(t); }
};



struct Bind_Data;

/**
 * Base class for basic representation of all data types.
 *
 * There are three methods:
 * - `stream_it()` to facilitate its use with the << operator, and
 * - 'get_size()` and `set_with_value()` save the data to memory.
 *   - Use the value from `get_size()` to allocate an appropriately-sized buffer,
 *     then invoke `set_with_value()` with the buffer and the size of the buffer.
 *   - For fixed-length data types like numbers, `get_size()` will return
 *     the appropriate data size.
 *   - For variable-length data types like strings, the `get_size()` will return a value
 *     that is 1 greater than the length of the data.  That leaves space to terminate the
 *     string with a \0.
 */
class BDType
{
public:
   virtual ~BDType() {}
   virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const = 0;
   virtual size_t get_size(const Bind_Data &bd) const = 0;
   virtual void set_with_value(const Bind_Data &bd, void* buff, size_t len) const = 0;
   virtual enum_field_types field_type(void) const = 0;
   virtual const char *type_name(void) const = 0;

   virtual bool is_unsigned(void) const = 0;
};

template <enum_field_types ftype, bool is_unsign=0>
class BDBase : public BDType
{
protected:
   const char *m_type_name;
public:
   BDBase(const char* tname) : m_type_name(tname) {}
   virtual ~BDBase() {}
   BDBase(const BDBase&) = delete;
   BDBase& operator=(const BDBase&) = delete;
   virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const = 0;
   virtual size_t get_size(const Bind_Data &bd) const = 0;
   virtual void set_with_value(const Bind_Data &bd, void* buff, size_t len) const = 0;
   inline virtual enum_field_types field_type(void) const { return ftype; }
   inline virtual const char *type_name(void) const { return m_type_name; }
   inline virtual bool is_unsigned(void) const { return is_unsign; }
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
   void set_with_value(void *buff, size_t len_buff) { bdtype->set_with_value(*this,buff,len_buff); }
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

inline const Bind_Data& get_bind_data(const Binder &b, int index) { return b.bind_data[index]; }
inline const BDType& get_bdtype(const Binder &b, int index) { return *get_bind_data(b,index).bdtype; }
inline const BDType& get_streamable(const Binder &b, int index) { return get_bdtype(b,index); }
inline bool is_null(const Binder &b, int index) { return get_bind_data(b,index).is_null; }
inline const char *type_name(const Binder &b, int index) { return get_bdtype(b,index).type_name(); }
inline const size_t get_size(const Binder &b, int index)
{
   const Bind_Data &bd = get_bind_data(b,index);
   return bd.bdtype->get_size(bd);
}
inline void set_with_value(const Binder &b, int index, void *buff, size_t len)
{
   const Bind_Data &bd = get_bind_data(b,index);
   bd.bdtype->set_with_value(bd, buff, len);
}




using IBinder_Callback = IGeneric_Callback<Binder>;
template <typename Func>
using Binder_User = Generic_User<Binder,Func>;

uint32_t get_bind_size(MYSQL_FIELD *fld);
void get_result_binds(MYSQL &mysql, IBinder_Callback &cb, MYSQL_STMT *stmt);

/**
 * Implementation of BDBase for fixed-length types
 */
template <typename T, enum_field_types ftype, bool is_unsign=0>
class BD_Num : public BDBase<ftype,is_unsign>
{
public:
   BD_Num(const char *tname) : BDBase<ftype,is_unsign>(tname) { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      os << *static_cast<T*>(bd.data);
      return os;
   }
   inline virtual size_t get_size(const Bind_Data &bd) const
   {
      return sizeof(T);
   }
   inline virtual void set_with_value(const Bind_Data &bd, void* buff, size_t len) const
   {
      memcpy(buff, bd.data, sizeof(T));
   }
};


template <enum_field_types strtype>
class BD_String : public BDBase<strtype>
{
public:
   BD_String(const char *tname) : BDBase<strtype>(tname) { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      os.write(static_cast<const char*>(bd.data), bd.len_data);
      return os;
   }

   inline virtual size_t get_size(const Bind_Data &bd) const
   {
      return bd.len_data+1;
   }
   inline virtual void set_with_value(const Bind_Data &bd, void* buff, size_t len) const
   {
      memcpy(buff, bd.data, bd.len_data);
      if (len > bd.len_data)
         static_cast<char*>(buff)[bd.len_data] = '\0';
   }
   
};





#endif

