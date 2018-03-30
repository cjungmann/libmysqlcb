
#include <mysql.h>
#include <iostream>
#include <string.h>  // For strlen()
#include <stdint.h>  // for uint32_t
#include <alloca.h>
#include "mysqlcb_binder.hpp"
#include "mysqlcb.hpp"

namespace mysqlcb {

const char *nullstr = static_cast<const char*>(nullptr);

void get_stack_string_args(IString_Callback &cb, va_list args)
{
   va_list counter;
   va_copy(counter, args);

   size_t buff_len = 1;

   while (1)
   {
      const char *str = va_arg(counter, const char*);
      if (str)
         buff_len += strlen(str);
      else
         break;
   }
   va_end(counter);

   char *buffer = static_cast<char*>(alloca(buff_len));
   char *ptr = buffer;
   size_t substr_len;
   while (1)
   {
      const char *str = va_arg(args, const char*);
      if (str)
      {
         substr_len = strlen(str);
         memcpy(ptr, str, substr_len);
         ptr += substr_len;
      }
      else
      {
         *ptr = '\0';
         break;
      }
   }

   cb(buffer);
}

void get_stack_string(IString_Callback &cb, ...)
{
   va_list args;
   va_start(args, cb);
   get_stack_string_args(cb, args);
   va_end(args);
}

void get_stack_string(void(*f)(const char*), ...)
{
   String_User<decltype(f)> su(f);
   va_list args;
   va_start(args, f);
   get_stack_string_args(su, args);
   va_end(args);
}



void throw_error(const char *err)
{
   throw std::runtime_error(err);
}

/**
 * Executes the query, then calls the callback function with each result row.
 *
 * @param mysql Handle to an open MySQL connection
 * @param cb    Callback function of type `void funcname(Binder &binder)`
 * @param query Text of the query
 *
 * @return void
 */
void execute_query(MYSQL &mysql, IBinder_Callback &cb, const char *query)
{
   MYSQL_STMT *stmt = mysql_stmt_init(&mysql);
   if (stmt)
   {
      int result = mysql_stmt_prepare(stmt, query, strlen(query));
      if (result==0)
      {
         result = mysql_stmt_execute(stmt);
         if (result==0)
         {
            auto f = [&mysql, &cb, &stmt](Binder &b)
            {
               mysql_stmt_bind_result(stmt, b.binds);

               int result;
               while ((result=mysql_stmt_fetch(stmt))!=MYSQL_NO_DATA)
               {
                  if (result==0)
                     cb(b);
                  else if (result==MYSQL_DATA_TRUNCATED)
                     std::cerr << "Got a truncated result...deal with it now." << std::endl;
               }
            };
            Binder_User<decltype(f)> bu(f);
            
            get_result_binds(mysql, bu, stmt);
         }
         else
            get_stack_string(throw_error,
                             "Failed to execute statement \"",
                             mysql_stmt_error(stmt),
                             "\"\n",
                             nullstr);
      }
      else
      {
         get_stack_string(throw_error,
                          "Failed to prepare statement \"",
                          mysql_stmt_error(stmt),
                          "\"\n",
                          nullstr);
      }

      mysql_stmt_close(stmt);
   }
   else
      get_stack_string(throw_error,
                       "Failed to initialize statement \"",
                       mysql_stmt_error(stmt),
                       "\"\n",
                       nullstr);
}

/**
 * Executes the query, then hands back a PullPack structure that includes a callback function
 * that gets a result row.  The function that receives the PullPack should call the included
 * pull function until it returns false, indicating that the final row has been retrieved.
 *
 * @param mysql Handle to an open MySQL connection
 * @param cb    Callback function of type `void funcname(PullPack &pp)
 * @param query Text of the query
 *
 * @return void
 */
   void int_execute_query_pull(MYSQL &mysql,
                               IPullPack_Callback &cb,
                               const char *query,
                               const Binder *binder)
{
   MYSQL_STMT *stmt = mysql_stmt_init(&mysql);
   if (stmt)
   {
      int result = mysql_stmt_prepare(stmt, query, strlen(query));

      if (result==0)
      {
         if (binder)
            result = mysql_stmt_bind_param(stmt, binder->binds);

         if (result)
         {
            get_stack_string(throw_error,
                             "Failed to bind parameters \"",
                             mysql_stmt_error(stmt),
                             "\"\n",
                             nullstr);
         }
         else
         {
            result = mysql_stmt_execute(stmt);
            if (result==0)
            {
               auto f = [&mysql, &cb, &stmt](Binder &binder)
                  {
                     mysql_stmt_bind_result(stmt, binder.binds);

                     int persist = 1;

                     auto puller = [&stmt, &binder, &persist](int go_on) -> int
                     {
                        int result;
                        do
                        {
                           result = mysql_stmt_fetch(stmt);
                           binder.bind_data->is_truncated = result==MYSQL_DATA_TRUNCATED;
                           persist = result!=MYSQL_NO_DATA;
                        }
                        while(go_on && persist);

                        return persist;
                     };
                     Puller_User<decltype(puller)> pu(puller);
                     PullPack pp = {mysql, binder, pu};

                     cb(pp);
                  };
               Binder_User<decltype(f)> bu(f);
            
               get_result_binds(mysql, bu, stmt);
            }
            else
               get_stack_string(throw_error,
                                "Failed to execute statement \"",
                                mysql_stmt_error(stmt),
                                "\"\n",
                                nullstr);
         }

      }
      else
      {
         get_stack_string(throw_error,
                          "Failed to prepare statement \"",
                          mysql_stmt_error(stmt),
                          "\"\n",
                          nullstr);
      }

      mysql_stmt_close(stmt);
   }
   else
      get_stack_string(throw_error,
                       "Failed to initialize statement \"",
                       mysql_stmt_error(stmt),
                       "\"\n",
                       nullstr);
}

void t_start_mysql(IMySQL_Callback &cb,
                   const char *host,
                   const char *user,
                   const char *pass,
                   const char *dbase)
{
   MYSQL mysql;

   // remainder of mysql_real_connect arguments:
   int           port = 0;
   const char    *socket = nullptr;
   unsigned long client_flag = 0;

   if (mysql_init(&mysql))
   {
      mysql_options(&mysql,MYSQL_READ_DEFAULT_FILE,"~/.my.cnf");
      mysql_options(&mysql,MYSQL_READ_DEFAULT_GROUP,"client");

      MYSQL *handle = mysql_real_connect(&mysql,
                                         host, user, pass, dbase,
                                         port, socket, client_flag);

      if (handle)
      {
         cb(mysql);
         mysql_close(&mysql);
      }
      else
      {
         get_stack_string(throw_error,
                          "MySQL connection failed \"",
                          mysql_error(&mysql),
                          "\"\n",
                          nullstr);
      }
   }
   else
      get_stack_string(throw_error,
                       "Failed to initialize MySQL: insufficient memory?\n",
                       nullstr);
}

void run_querier_pack(IQuerier_Callback &cb, MYSQL &mysql)
{
   bool  in_query = false;
   static const char msg_osync[] = "Old query must run to completion before start a new query.\n";

   auto fpusher = [&mysql, &in_query](IBinder_Callback &cb, const char *query) -> void
      {
         if (in_query)
         {
            std::cerr << msg_osync;
         }
         else
         {
            in_query = true;
            execute_query(mysql, cb, query);
            in_query = false;
         }
      };
   PushQuery_User<decltype(fpusher)> pushercb(fpusher);

   auto fpuller = [&mysql, &in_query](IPullPack_Callback &cb, const char *query) -> void
      {
         if (in_query)
         {
            std::cerr << msg_osync;
         }
         else
         {
            in_query = true;
            execute_query_pull(mysql, cb, query);
            in_query = false;
         }
      };
   PullQuery_User<decltype(fpuller)> pullercb(fpuller);

   Querier_Pack qp = {pushercb, pullercb};

   cb(qp);
}

void t_get_querier_pack(IQuerier_Callback &cb,
                        const char *host, const char *user, const char *pass, const char *dbase)
{
   auto f = [&cb](MYSQL &mysql)
   {
      run_querier_pack(cb, mysql);
   };
   MySQL_User<decltype(f)> su(f);

   start_mysql(su, host, user, pass, dbase);
}

} // namespace
