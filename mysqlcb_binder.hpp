#ifndef MYSQLCB_BINDER_HPP_SOURCE
#define MYSQLCB_BINDER_HPP_SOURCE

#include <mysql.h>
#include <stdint.h>  // for uint32_t
#include <string.h>  // for memcpy()
#include <assert.h>
#include <iostream>
#include <iomanip>   // setw, setfill, etc.

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
   virtual IGeneric_Callback &operator=(const IGeneric_Callback &) = delete;
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
   bool name_match(const char *name) const { return 0==strcasecmp(name,type_name()); }
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
   virtual        std::ostream&   stream_it(std::ostream &os, const Bind_Data &bd) const = 0;
   virtual        size_t          get_size(const Bind_Data &bd) const = 0;
   virtual        void            set_with_value(const Bind_Data &bd, void* buff, size_t len) const = 0;
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

   bool          is_truncated;

   MYSQL_FIELD   *field;
   MYSQL_BIND    *bind;
   const BDType  *bdtype;
};

inline std::ostream& operator<<(std::ostream &os, const Bind_Data &obj)
{
   return obj.bdtype->stream_it(os, obj);
}

inline std::ostream& operator<<(std::ostream &os, const Bind_Data *obj)
{
   return obj->bdtype->stream_it(os, *obj);
}

inline bool valid(const Bind_Data &bd) { return bd.field != nullptr; }
inline bool valid(const Bind_Data *bd) { return bd->field != nullptr; }
inline bool is_unsupported_type(const Bind_Data& bd) { return bd.bdtype==nullptr; }
inline uint32_t required_buffer_size(const Bind_Data& bd) { return bd.bdtype->get_size(bd); }

inline size_t get_size(const Bind_Data &bd) { return bd.bdtype->get_size(bd); }
inline size_t get_size(const Bind_Data *bd) { return bd->bdtype->get_size(*bd); }
inline void set_with_value(const Bind_Data &bd, void *buff, size_t len_buff)
{
   bd.bdtype->set_with_value(bd, buff, len_buff);
}
inline void set_with_value(const Bind_Data *bd, void *buff, size_t len_buff)
{
   bd->bdtype->set_with_value(*bd, buff, len_buff);
}

inline const char * field_name(const Bind_Data &bd) { return bd.field->name; }
inline const char * field_name(const Bind_Data *bd) { return bd->field->name; }
inline const char * type_name(const Bind_Data &bd) { return bd.bdtype->type_name(); }
inline const char * type_name(const Bind_Data *bd) { return bd->bdtype->type_name(); }
inline bool is_null(const Bind_Data &bd) { return bd.is_null; }
inline bool is_null(const Bind_Data *bd) { return bd->is_null; }



/**
 * bundle of Field information, collected from mysql_stmt_result_metadata.
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

// inline const Bind_Data& get_bind_data(const Binder &b, int index { return b.bind_data[index]; }
// inline const BDType& get_bdtype(const Binder &b, int index) { return *get_bind_data(b,index).bdtype; }
// inline const BDType& get_streamable(const Binder &b, int index) { return get_bdtype(b,index); }
// inline bool is_null(const Binder &b, int index) { return get_bind_data(b,index).is_null; }
// inline const char *type_name(const Binder &b, int index) { return get_bdtype(b,index).type_name(); }
// inline const size_t get_size(const Binder &b, int index)
// {
//    const Bind_Data &bd = get_bind_data(b,index);
//    return bd.bdtype->get_size(bd);
// }
// inline void set_with_value(const Binder &b, int index, void *buff, size_t len)
// {
//    const Bind_Data &bd = get_bind_data(b,index);
//    bd.bdtype->set_with_value(bd, buff, len);
// }




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

template <enum_field_types ftype>
class BD_DateBase : public BDBase<ftype>
{
public:
   BD_DateBase(const char *tname) : BDBase<ftype>(tname) { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const=0;
   inline virtual size_t get_size(const Bind_Data &bd) const {return sizeof(MYSQL_TIME);}
   inline virtual void set_with_value(const Bind_Data &bd, void* buff, size_t len) const
   {
      memcpy(buff, bd.data, sizeof(MYSQL_TIME));
   }
};

class BD_Date : public BD_DateBase<MYSQL_TYPE_DATE>
{
public:
   BD_Date(void) : BD_DateBase<MYSQL_TYPE_DATE>("DATE") { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      const MYSQL_TIME &date = *static_cast<const MYSQL_TIME*>(bd.data);
      os << std::setfill('0')
         << std::setw(4) << date.year
         << "-" << std::setw(2) << date.month
         << "-" << std::setw(2) << date.day;
      return os;
   }
};

template <enum_field_types ftype>
class BD_DateTimeBase : public BD_DateBase<ftype>
{
public:
   BD_DateTimeBase(const char *name) : BD_DateBase<ftype>(name) { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      const MYSQL_TIME &date = *static_cast<const MYSQL_TIME*>(bd.data);
      os << std::setfill('0')
         << std::setw(4) << date.year
         << "-" << std::setw(2) << date.month
         << "-" << std::setw(2) << date.day
         << " " << std::setw(2) << date.hour
         << ":" << std::setw(2) << date.minute
         << ":" << std::setw(2) << date.second;
      return os;
   }
};

using BD_DateTime = BD_DateTimeBase<MYSQL_TYPE_DATETIME>;
using BD_TimeStamp = BD_DateTimeBase<MYSQL_TYPE_TIMESTAMP>;

class BD_Time : public BD_DateBase<MYSQL_TYPE_TIME>
{
public:
   BD_Time(void) : BD_DateBase<MYSQL_TYPE_TIME>("TIME") { }
   inline virtual std::ostream& stream_it(std::ostream &os, const Bind_Data &bd) const
   {
      const MYSQL_TIME &date = *static_cast<const MYSQL_TIME*>(bd.data);
      os << std::setfill('0')
         << std::setw(2) << date.hour
         << ":" << std::setw(2) << date.minute
         << ":" << std::setw(2) << date.second;
      return os;
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
      {
         char *ptr = static_cast<char*>(buff);
         ptr += bd.len_data;
         *ptr = '\0';
         // static_cast<char*>(buff)[bd.len_data] = '\0';
      }
   }
   
};


extern const BD_String<MYSQL_TYPE_VAR_STRING> bd_VarString;
const BDType *get_bdtype(const MYSQL_FIELD &fld);
const BDType *get_bdtype(const char *name);

bool confirm_bdtype(const BDType *t);

/** */
class BaseParam
{
protected:
   size_t       m_len;
   const void   *m_data;
   const BDType *m_type;
public:
   BaseParam(const void *d, size_t len, const BDType *type)
      : m_len(len), m_data(d), m_type(type){}
   bool is_void(void) const                { return m_data==nullptr && m_type==nullptr; }

   size_t       len(void) const            { return m_len; }
   size_t  get_size(void) const            { return m_len; }
   const void*  data(void) const           { return m_data; }
   const BDType &type(void) const          { return *m_type; }
   enum_field_types field_type(void) const { return m_type->field_type(); }
   bool is_unsigned(void) const            { return m_type->is_unsigned(); }
};

/** */
template <typename T>
class FixedParam : public BaseParam
{
public:
   FixedParam(const char *type_name, T &val)
      : BaseParam(static_cast<const void*>(&val), sizeof(T), get_bdtype(type_name)) { }
};

class StringParam : public BaseParam
{
public:
   StringParam(const char *val)
      : BaseParam(static_cast<const void*>(val), 1+strlen(val), &bd_VarString) { }
};

class VoidParam : public BaseParam
{
public:
   VoidParam(void) : BaseParam(nullptr, 0, nullptr) { }
};


void summon_binder(IBinder_Callback &cb,...);


class MParam
{
protected:
   size_t       m_size;
   const void   *m_data;
   const BDType *m_type;
public:
   MParam(void) : m_size(0), m_data(nullptr), m_type(nullptr) { }
   MParam(const char *str);
   MParam(int &val);
   MParam(unsigned int &val);
   MParam(double &val);

   inline bool         is_null(void) const      { return m_size==0; }
   inline bool         is_valid(void) const     { return m_size!=0; assert(confirm_bdtype(m_type)); }
   inline size_t       size(void) const         { return m_size; }
   inline const void   *data(void) const        { return m_data; }
   inline const BDType *type(void) const        { return m_type; }
   enum_field_types    field_type(void) const   { return m_type->field_type(); }
   bool                 is_unsigned(void) const { return m_type->is_unsigned(); }
};

void summon_binder(IBinder_Callback &cb, const MParam *param);


#endif

