#ifndef MYSQLCB_HPP_SOURCE
#define MYSQLCB_HPP_SOURCE

#include <mysql.h>   // For MySQL definitions
#include <stdint.h>  // for uint32_t
#include "mysqlcb_binder.hpp"

/** *************** */
class IPuller_Callback
{
public:
   virtual ~IPuller_Callback() {}
   virtual int operator()(int persist) const = 0;
};

/** ***************** */
template <typename Func>
class Puller_User : public IPuller_Callback
{
protected:
   const Func &m_f;
public:
   Puller_User(const Func &f) : m_f(f)       {}
   virtual ~Puller_User()                    {}
   virtual int operator()(int persist) const { return m_f(persist); }
};


/** ******** */
struct PullPack
{
   MYSQL            &mysql;
   Binder           &binder;
   IPuller_Callback &puller;
};

/** ***************** */
class IPullPack_Callback
{
public:
   virtual ~IPullPack_Callback() {}
   virtual void operator()(PullPack &b) const=0;
};

/** ***************** */
template <typename Func>
class PullPack_User : public IPullPack_Callback
{
protected:
   const Func &m_f;
public:
   PullPack_User(const Func &f) : m_f(f)      {}
   virtual ~PullPack_User()                   {}
   virtual void operator()(PullPack &b) const { m_f(b); }
};

/** ****************** */
class IPullQuery_Callback
{
public:
   virtual ~IPullQuery_Callback() {}
   virtual void operator()(IPullPack_Callback &cb, const char *query, ...) const = 0;
};

template <typename Func>
class PullQuery_User : public IPullQuery_Callback
{
protected:
   const Func &m_f;
public:
   PullQuery_User(Func &f) : m_f(f) {}
   virtual ~PullQuery_User()       {}
   virtual void operator()(IPullPack_Callback &cb, const char *query, ...) const
   {
      va_list arglist;
      va_start(arglist, query);
      m_f(cb, query, arglist);
      va_end(arglist);
   }
};

/** ****************** */
class IPushQuery_Callback
{
public:
   virtual ~IPushQuery_Callback() {}
   virtual void operator()(IBinder_Callback &cb, const char *query, ...) const = 0;
};

/** ****************** */
template <typename Func>
class PushQuery_User : public IPushQuery_Callback
{
protected:
   const Func &m_f;
public:
   PushQuery_User(Func &f) : m_f(f) {}
   virtual ~PushQuery_User()        {}
   virtual void operator()(IBinder_Callback &cb, const char *query, ...) const
   {
      va_list arglist;
      va_start(arglist, query);
      m_f(cb, query, arglist);
      va_end(arglist);
   }
};

struct Querier_Pack
{
   const IPushQuery_Callback &pushcb;
   const IPullQuery_Callback &pullcb;
};

/** **************** */
class IQuerier_Callback
{
public:
   virtual ~IQuerier_Callback() {}
   virtual void operator()(const Querier_Pack &qp) const = 0;
};

template <typename Func>
class Querier_User : public IQuerier_Callback
{
protected:
   const Func &m_f;
public:
   Querier_User(const Func &f) : m_f(f)            {}
   virtual ~Querier_User()                         {}
   virtual void operator()(const Querier_Pack &qp) { m_f(qp); }
};


class IConnection_Callback
{
public:
   virtual ~IConnection_Callback() {}
   virtual void operator()(Querier_Pack &qp) const = 0;
};

template <typename Func>
class Connection_User : public IConnection_Callback
{
protected:
   const Func &m_f;
public:
   Connection_User(const Func &f) : m_f(f)              {}
   virtual ~Connection_User()                           {}
   virtual void operator()(Querier_Pack &cb) const { m_f(cb); }
};



/**
 * When instatiating the following template classes, it is necessary
 * to use the `decltype` function for the function template argument.
 * 
 * Consult the following example for guidance:
 *~~~c++
 void yourfunc(MYSQL &mysql)
{
   auto f = [&mysql](Binder &binder)
   {
      // Use the binder stuff here
   };
   Binder_User<decltype(f)> bu(f);
   execute_query(mysql, bu, "SELECT * FROM People");
}
 *~~~
 */
// template <typename Func>
// using Binder_User = Generic_User<Binder,Func>;
// template <typename Func>
// using PullPack_User = Generic_User<PullPack,Func>;

// template <class Func>
// using Puller_User = Generic_UserReturn<int, int, Func>;

// using IPullPack_Callback = IGeneric_Callback<PullPack>;

// template <class Func>
// using PullPack_User = Generic_User<PullPack, Func>;

// Function pointer callback that works like IBinder_Callback
typedef void (*binder_callback)(Binder &b);
typedef void (*pullpack_callback)(PullPack &p);


/**
 * Functions for getting mysql rows.
 *
 * The functions for getting mysql rows are of two types, push or pull.
 * - The **push** functions call the callback function for each row of a 
 *   query result.
 * - The **pull** function returns a structure that includes a reference to
 *   the Binder that is bound to the statment **and** a callback function
 *   that is used to *pull* results at the programmer's convenience.
 *
 * The *push* functions are two execute_query() functions will invoke the
 * callback function for each result row.  The difference between them is
 * the second parameter, one uses a template class that can wrap a lambda
 * for * normal function.  The other execute_query() uses a normal function
 * for callback.
 *
 * The *pull* functions are the two variations of `execute_query_pull()`,
 * whose second parameters are either a template class or a callback function.
 */

void execute_query(MYSQL &mysql, IBinder_Callback &cb, const char *query, va_list list=nullptr);

/**
 * Call execute_query callback function.
 *
 * Internally this function calls the lambda function, which is more flexible.
 */
inline void execute_query(MYSQL &mysql, binder_callback cb, const char *query, va_list list=nullptr)
{
   Binder_User<binder_callback> bu(cb);
   execute_query(mysql, bu, query, list);
}

void execute_query_pull(MYSQL &mysql, IPullPack_Callback &cb, const char *query, va_list list=nullptr);
inline void execute_query_pull(MYSQL &mysql, pullpack_callback cb, const char *query, va_list list=nullptr)
{
   PullPack_User<pullpack_callback> bu(cb);
   execute_query_pull(mysql, bu, query, list);
}

void start_mysql(IConnection_Callback &cb,
                 const char *host=nullptr,
                 const char *user=nullptr,
                 const char *pass=nullptr);



#endif

