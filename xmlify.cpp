#include <mysql.h>
#include <iostream>

#include "mysqlcb.hpp"

/** Adds attributes to a schema field element for set flags. */
void add_field_attributes_from_flags(unsigned int flags)
{
   if (flags & NOT_NULL_FLAG)
      std::cout << " not_null=\"true\"";
   if (flags & PRI_KEY_FLAG)
      std::cout << " primary_key=\"true\"";
   if (flags & UNIQUE_KEY_FLAG)
      std::cout << " unique_key=\"true\"";
   if (flags & MULTIPLE_KEY_FLAG)
      std::cout << " multiple_key=\"true\"";
   if (flags & BLOB_FLAG)
      std::cout << " blob=\"true\"";
   if (flags & UNSIGNED_FLAG)
      std::cout << " unsigned=\"true\"";
   if (flags & ZEROFILL_FLAG)
      std::cout << " zero_fill=\"true\"";
   if (flags & BINARY_FLAG)
      std::cout << " binary=\"true\"";
   if (flags & ENUM_FLAG)
      std::cout << " enum=\"true\"";
   if (flags & AUTO_INCREMENT_FLAG)
      std::cout << " auto_increment=\"true\"";
   if (flags & TIMESTAMP_FLAG)
      std::cout << " timestamp=\"true\"";
   if (flags & SET_FLAG)
      std::cout << " set=\"true\"";
}

void print_schema(Binder &b)
{
   std::cout << "<schema>\n";

   const Bind_Data *bd = b.bind_data;
   while (valid(bd))
   {
      std::cout << "<field name=\"" << field_name(bd) << "\" type=\"" << type_name(bd) << "\"";
      add_field_attributes_from_flags(bd->field->flags);
      std::cout << " />\n";

      ++bd;
   }
   std::cout << "</schema>\n";
}

/**
 * Uses libmysqlcb functions to open a database, run a query, and output the result as XML.
 *
 * This function is a demonstration of the library using the **push** strategy.  The library
 * executes the *push* method by invoking a callback function for each row in the query
 * result.
 *
 * In this example, the callback function is 
 */
void run_query(const char *query,
               const char *host,
               const char *user,
               const char *password,
               const char *dbase,
               bool include_schema)
{
   auto xmlify = [&include_schema](Binder &b)
   {
      if (include_schema)
      {
         print_schema(b);
         include_schema = 0;
      }
         
      std::cout << "<row";

      const Bind_Data *bd = b.bind_data;
      while (valid(bd))
      {
         if (!is_null(bd))
            std::cout << " " << field_name(bd) << "=\"" << bd << "\"";
         ++bd;
      }

      std::cout << "/>\n";
   };

   auto fqp = [&query, &xmlify](Querier_Pack &qp)
   {
      std::cout << "<?xml version=\"1.0\" ?>\n<resultset>\n";
      start_push(qp, xmlify, query);
      std::cout << "</resultset>\n";
   };

   start_mysql(fqp, host, user, password, dbase);
}

void show_usage(void)
{
   std::cout << "Usage instructions:\n\n"
      "xmlify [-u USER] [-pPASSWORD] [-h HOST] [-e SQL_STATEMENT] [-s] database_name\n\n"
      "Options:\n"
      "-e sql statement\n"
      "   SQL statement that should be executed.\n"
      "-h [host]\n"
      "   MYSQL host to which the connection is to be made.\n"
      "   If this option is omitted, localhost will be used.\n"
      "--help\n"
      "   This usage display.\n"
      "-p[password]\n"
      "   Password for user account.  Note there is no space between -p and the password value.\n"
      "-s\n"
      "   Include a schema in the output.\n"
      "-u [user]\n"
      "   User account to use for the connection.\n"
      "\n"
      "The utility will use default values under [client] in ~/.my.cnf for unspecified options.\n"
      "\n"
      "The options can go before or after the required database name.\n"
      "\n"
      "In practice, the -e option is required for output.\n";
}

int main(int argc, char **argv)
{
   const char *dbase = nullptr;
   const char *query = nullptr;
   const char *host = "localhost";
   const char *user = nullptr;
   const char *password = nullptr;
   bool include_schema = 0;
   bool display_usage = 0;

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
            case '-':
               arg += 2;
               if (0==strcmp(arg,"help"))
               {
                  i = argc;
                  display_usage = true;
               }
            case 'e':
               query = argv[++i];
               break;
            case 'h':
               host = argv[++i];
               break;
            case 'p':
               password = &arg[2];
               break;
            case 's':
               include_schema = 1;
               break;
            case 'u':
               user = argv[++i];
               break;
            default:
               i = argc;
               display_usage = true;
               break;
         }
      }
      else
         dbase = arg;
   }

   if (display_usage)
      show_usage();
   else
      run_query(query, host, user, password, dbase, include_schema);

   return 0;
}
