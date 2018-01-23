
#include <mysql.h>
#include <iostream>
#include <string.h>  // For strlen()
#include <stdint.h>  // for uint32_t
#include <alloca.h>
#include "mysqlcb_binder.hpp"
#include "mysqlcb.hpp"
using namespace std;


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
                     cerr << "Got a truncated result...deal with it now." << endl;
               }
            };
            Binder_User<decltype(f)> bu(f);
            
            get_result_binds(mysql, bu, stmt);
         }
         else
            cerr << "Failed to execute statement \"" << mysql_stmt_error(stmt) << "\"" << endl;
      }
      else
      {
         cerr << "Failed to prepare statement \"" << mysql_error(&mysql) << "\"" << endl;
      }

      mysql_stmt_close(stmt);
   }
   else
      cerr << "Failed to initialize the statement." << endl;
}

void execute_query_pull(MYSQL &mysql, IPullPack_Callback &cb, const char *query)
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
            cerr << "Failed to execute statement \"" << mysql_stmt_error(stmt) << "\"" << endl;
      }
      else
      {
         cerr << "Failed to prepare statement \"" << mysql_error(&mysql) << "\"" << endl;
      }

      mysql_stmt_close(stmt);
   }
   else
      cerr << "Failed to initialize the statement." << endl;
}

