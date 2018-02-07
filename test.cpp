#include <mysql.h>
#include <iostream>

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

void xmlify(Binder &b)
{
   cout << "<row";
   for (uint32_t i=0; i<b.field_count; ++i)
   {
      cout << " " <<  b.fields[i].name << "=\"" << b.bind_data[i] << "\"";
   }
   cout << "/>\n";
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
   // PullPack_User<decltype(fpuller)> pu(fpuller);


   /** Pusher method user lambda **/
   auto fpusher = [](Binder &b)
   {
      for (uint32_t i=0; i<b.field_count; ++i)
      {
         cout << b.fields[i].name << ": " << static_cast<const char*>(b.binds[i].buffer) << endl;
      }
   };
   // Binder_User<decltype(fpusher)> bu(fpusher);

   /** Pusher method callback lambda **/
   auto fqp = [&fpusher, &fpuller](Querier_Pack &qp)
   {
      start_pull(qp, fpuller, "SELECT SCHEMA_NAME FROM SCHEMATA");
      start_push(qp, fpusher, "SELECT SCHEMA_NAME FROM SCHEMATA");

      start_push(qp, xmlify, "SELECT SCHEMA_NAME FROM SCHEMATA");
   };

   get_querier_pack(fqp);
}
  



int main(int argc, char **argv)
{
   start_app();
   return 0;
}

