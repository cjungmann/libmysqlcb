
#include <mysql.h>
#include <iostream>
#include <string.h>  // For strlen()
#include <stdint.h>  // for uint32_t
#include <alloca.h>
#include "mysqlcb_binder.hpp"
#include "mysqlcb.hpp"

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
            std::cerr << "Failed to execute statement \"" << mysql_stmt_error(stmt) << "\"" << std::endl;
      }
      else
      {
         std::cerr << "Failed to prepare statement \"" << mysql_error(&mysql) << "\"" << std::endl;
      }

      mysql_stmt_close(stmt);
   }
   else
      std::cerr << "Failed to initialize the statement." << std::endl;
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
void execute_query_pull(MYSQL &mysql,
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
            std::cerr << "Failed to bind parameters \"" << mysql_stmt_error(stmt) << "\"" << std::endl;
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
               std::cerr << "Failed to execute statement \"" << mysql_stmt_error(stmt) << "\"" << std::endl;
         }

      }
      else
      {
         std::cerr << "Failed to prepare statement \"" << mysql_error(&mysql) << "\"" << std::endl;
      }

      mysql_stmt_close(stmt);
   }
   else
      std::cerr << "Failed to initialize the statement." << std::endl;
}

void t_start_mysql(IConnection_Callback &cb,
                   const char *host,
                   const char *user,
                   const char *pass,
                   const char *dbase)
{
   MYSQL mysql;
   bool  in_query = false;
   static const char msg_osync[] = "Old query must run to completion before start a new query.\n";

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

         // use_mysql(mysql);
         mysql_close(&mysql);
      }
      else
      {
         std::cerr << "MySQL connection failed: " << mysql_error(&mysql) << std::endl;
      }
   }
   else
      std::cerr << "Failed to initialize MySQL: insufficient memory?\n";
   
}

