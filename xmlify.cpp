#include <mysql.h>
#include <iostream>

using namespace std;

#include "mysqlcb.hpp"

void xmlify(Binder &b)
{
   cout << "<row";

   for (uint32_t i=0; i<b.field_count; ++i)
   {
      if (!is_null(b,i))
         cout << " " <<  b.fields[i].name << "=\"" << b.bind_data[i] << "\"";
   }
   cout << "/>\n";
}


void run_query(const char *query,
               const char *host,
               const char *user,
               const char *password,
               const char *dbase)
{
   auto fqp = [&query](Querier_Pack &qp)
   {
      std::cout << "<?xml version=\"1.0\" ?>\n<resultset>\n";
      push(qp, xmlify, query);
      std::cout << "</resultset>\n";
   };

   start_mysql(fqp, host, user, password, dbase);
}

void show_usage(void)
{
   std::cout << "Usage instructions:\n\n"
      "xmlify [options] dbname\n\n"
      "Options:\n"
      "-e [sql statement]\n"
      "-h [host]\n"
      "-p[password]\n"
      "-u [user]\n";
}

int main(int argc, char **argv)
{
   const char *dbase = nullptr;
   const char *query = nullptr;
   const char *host = nullptr;
   const char *user = nullptr;
   const char *password = nullptr;

   if (argc<2)
   {
      show_usage();
      return 1;
   }

   for (int i=1; i<argc; ++i)
   {
      char *arg = argv[i];
      if (*arg=='-')
      {
         switch(arg[1])
         {
            case 'e':
               query = argv[++i];
               break;
            case 'h':
               host = argv[++i];
               break;
            case 'p':
               password = &arg[2];
               break;
            case 'u':
               user = argv[++i];
               break;
            default:
               break;
         }
      }
      else
         dbase = arg;
   }

   run_query(query, host, user, password, dbase);


   return 0;
}
