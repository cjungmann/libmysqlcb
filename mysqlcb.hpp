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

using IPullPack_Callback = IGeneric_Callback<PullPack>;
template <typename Func>
using PullPack_User = Generic_User<PullPack,Func>;


/**
 * The function-call method of Query_Callback takes two parameters,
 * a callback AND a const char*, so it needs new template.
 *
 * Thus, IQuery_Callback and Query_User are template classes for
 * IPullQuery_Callback and PullQuery_User and
 * IPushQuery_Callback and PushQuery_User.
 */
template <class CB>
class IQuery_Callback
{
public:
   virtual ~IQuery_Callback() {}
   virtual void operator()(CB &cb, const char *query) const = 0;
};

template <class CB, typename Func>
class Query_User : public IQuery_Callback<CB>
{
protected:
   const Func &m_f;
public:
   Query_User(Func &f) : m_f(f) {}
   virtual ~Query_User()        {}
   virtual void operator()(CB &cb, const char *query) const
   {
      m_f(cb,query);
   }
};

using IPullQuery_Callback = IQuery_Callback<IPullPack_Callback>;
template <typename Func>
using PullQuery_User = Query_User<IPullPack_Callback, Func>;

using IPushQuery_Callback = IQuery_Callback<IBinder_Callback>;
template <typename Func>
using PushQuery_User = Query_User<IBinder_Callback, Func>;

struct Querier_Pack
{
   const IPushQuery_Callback &pushcb;
   const IPullQuery_Callback &pullcb;
};

template <typename Func>
inline void start_push(const Querier_Pack &qp, Func f, const char *query)
{
   Binder_User<Func> bu(f);
   qp.pushcb(bu, query);
}

template <typename Func>
inline void start_pull(const Querier_Pack &qp, Func f, const char *query)
{
   PullPack_User<Func> pu(f);
   qp.pullcb(pu, query);
}

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
/** @file */

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

void execute_query(MYSQL &mysql, IBinder_Callback &cb, const char *query);

/**
 * Call execute_query callback function.
 *
 * Internally this function calls the lambda function, which is more flexible.
 */
inline void execute_query(MYSQL &mysql, binder_callback cb, const char *query)
{
   Binder_User<binder_callback> bu(cb);
   execute_query(mysql, bu, query);
}

void execute_query_pull(MYSQL &mysql, IPullPack_Callback &cb, const char *query);
inline void execute_query_pull(MYSQL &mysql, pullpack_callback cb, const char *query)
{
   PullPack_User<pullpack_callback> bu(cb);
   execute_query_pull(mysql, bu, query);
}

void t_start_mysql(IConnection_Callback &cb,
                   const char *host=nullptr,
                   const char *user=nullptr,
                   const char *pass=nullptr,
                   const char *dbase=nullptr);

template <typename Func>
void start_mysql(Func &cb,
                 const char *host=nullptr,
                 const char *user=nullptr,
                 const char *pass=nullptr,
                 const char *dbase=nullptr)
{
   Connection_User<Func> cu(cb);
   t_start_mysql(cu,host,user,pass,dbase);
}



#endif

