#include <mysql.h>
#include <mysql.h>
#include <iostream>
#include <string.h>  // For strlen()
#include <stdint.h>  // for uint32_t

using namespace std;

#include "mysqlcb.hpp"


void use_row(Binder &b)
{
   for (uint32_t i=0; i<b.field_count; ++i)
   {
      cout << b.fields[i].name << ": " << static_cast<const char*>(b.binds[i].buffer) << endl;
   }
}

void use_mysql(MYSQL &mysql)
{
   Binder_User<decltype(use_row)> bu(use_row);
   execute_query(mysql, bu, "SELECT SCHEMA_NAME FROM SCHEMATA");
}


bool start_mysql(const char *host=nullptr,
                 const char *user=nullptr,
                 const char *pass=nullptr)
{
   MYSQL mysql;

   // remainder of mysql_real_connect arguments:
   const char    *db = "information_schema";
   int           port = 0;
   const char    *socket = nullptr;
   unsigned long client_flag = 0;
     

   mysql_init(&mysql);
   mysql_options(&mysql,MYSQL_READ_DEFAULT_FILE,"~/.my.cnf");
   mysql_options(&mysql,MYSQL_READ_DEFAULT_GROUP,"client");

   MYSQL *handle = mysql_real_connect(&mysql,
                                      host, user, pass, db,
                                      port, socket, client_flag);

   if (handle)
   {
      use_mysql(mysql);
      mysql_close(&mysql);
      return 1;
   }
   else
   {
      cerr << "Failed to start MYSQL: " << mysql_error(&mysql) << endl;
      return 0;
   }
   
}



int main(int argc, char **argv)
{
   start_mysql();
   return 0;
}

