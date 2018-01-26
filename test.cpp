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

void use_mysql_pusher(MYSQL &mysql)
{
   Binder_User<decltype(use_row)> bu(use_row);
   execute_query(mysql, bu, "SELECT SCHEMA_NAME FROM SCHEMATA");
}

void use_mysql_puller(PullPack &pp)
{
   Binder &binder = pp.binder;
   Bind_Data &bdata = binder.bind_data[0];
   while (pp.puller(false))
   {
      std::cout << "Pulled a " << bdata << std::endl;
   };
}

void use_mysql_puller(MYSQL &mysql)
{
   auto f = [](PullPack &pp)
   {
      Binder &binder = pp.binder;
      Bind_Data &bdata = binder.bind_data[0];
      while (pp.puller(false))
      {
         std::cout << "Pulled a " << bdata << std::endl;
      };
   };
   PullPack_User<decltype(f)> pu(f);

   execute_query_pull(mysql, pu, "SELECT SCHEMA_NAME FROM SCHEMATA");
}


void start_app(void)
{
   /** Puller method user lambda **/
   auto fpuller = [](PullPack &pp)
   {
      Binder &binder = pp.binder;
      Bind_Data &bdata = binder.bind_data[0];

      while (pp.puller(false))
      {
         std::cout << bdata << std::endl;
      }
   };
   PullPack_User<decltype(fpuller)> pu(fpuller);


   /** Pusher method user lambda **/
   auto fpusher = [](Binder &b)
   {
      for (uint32_t i=0; i<b.field_count; ++i)
      {
         cout << b.fields[i].name << ": " << static_cast<const char*>(b.binds[i].buffer) << endl;
      }
   };
   Binder_User<decltype(fpusher)> bu(fpusher);


   /** Pusher method callback lambda **/
   auto fqp = [&pu, &bu](Querier_Pack &qp)
   {
      qp.pullcb(pu, "SELECT SCHEMA_NAME FROM SCHEMATA");
      qp.pushcb(bu, "SELECT SCHEMA_NAME FROM SCHEMATA");
   };
   
   /** Push method start_mysql user callback **/
   Connection_User<decltype(fqp)> cu(fqp);
   

   start_mysql(cu);
}
  



int main(int argc, char **argv)
{
   // start_mysql();
   start_app();
   return 0;
}

