#include <mysql.h>
#include <iostream>

#include "mysqlcb.hpp"

using namespace mysqlcb;

const char table_schema_query[] =
   "SELECT COLUMN_NAME,"
   " UPPER(DATA_TYPE), "
   " CHARACTER_MAXIMUM_LENGTH, "
   " COLUMN_TYPE,"
   " NULLIF('0',INSTR(EXTRA,'auto_increment')) AS autoinc,"
   " NULLIF('0',INSTR(COLUMN_KEY,'PRI')) AS prikey,"
   " NULLIF(IS_NULLABLE,'YES') AS nullable"
   " FROM COLUMNS"
   " WHERE TABLE_SCHEMA=? AND TABLE_NAME=?";

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

bool is_int_type(const Bind_Data &data_type)
{
   size_t len = get_size(data_type);
   const char *str = static_cast<const char*>(data_type.data);
   return 0==strncmp("INT", &str[len-3], 3);
}

bool is_enum_type(const Bind_Data &data_type)
{
   const char *str = static_cast<const char*>(data_type.data);
   return 0==strncmp("ENUM", str, 4);
}

bool is_set_type(const Bind_Data &data_type)
{
   const char *str = static_cast<const char*>(data_type.data);
   return 0==strncmp("SET", str, 3);
}

bool is_unsigned_type(const Bind_Data &column_type)
{
   size_t len = get_size(column_type);
   if (len > 8)
   {
      const char *str = static_cast<const char*>(column_type.data);
      return 0==strncmp("int", &str[len-8], 8);
   }
   else
      return false;
}

/**
 * Writes string to stdout, converting &"'<> to XML entities and handling double ' or "
 *
 * The character that *end points to should be the quoted string terminator or nullptr;
 * When the function encounters either ' or ", it will compare it to the char at *end.
 * If it matches, duplicates will be supressed. If not matched, adjacent ' or " will
 * both print.
 */
void xmlify_sql_string(const char *start, const char *end=nullptr)
{
   if (end==nullptr)
      end = start + strlen(start);

   while(start<end)
   {
      switch(*start)
      {
         case '&':
            std::cout << "&amp;";
            break;
         case '<':
            std::cout << "&lt;";
            break;
         case '>':
            std::cout << "&gt;";
            break;
         case '"':
            std::cout << "&quot;";
            break;
         case '\'':
            std::cout << "&apos;";
            break;
         default:
            std::cout.put(*start);
            break;
      }

      // Skip duplicate if it is string's enclosing character
      if (*start==*end && *(start+1)==*end)
         ++start;

      ++start;
   }
}

void add_value_list(const Bind_Data &column_type)
{
   const char *str = static_cast<const char *>(column_type.data);
   const char *end = str + get_size(column_type);

   const char *p = str;
   const char *val = nullptr;

   // Skip up to just-past the parenthesis.  Use the
   // post-increment test for '(' to position pointer.
   while(p < end && *p++!='(')
      ;

   while (p < end && *p == '\'')
   {
      val = ++p;

      // Breaks out with terminating '
      while (++p<end)
      {
         if (*p=='\'')         // terminating '?
         {
            if (*++p == '\'')  // ignore double 's
               continue;
            else
            {
               assert(*p==',' || *p==')');

               std::cout << "   <item val=\"";
               xmlify_sql_string(val, p-1); // send last ' as terminator
               std::cout << "\" />\n";
               val = nullptr;
               
               ++p;
               break;
            }
         }
      }
   }
}

void print_columns_as_fields(const PullPack &pp)
{
   Bind_Data *bd = pp.binder.bind_data;
   Bind_Data &bName     = bd[0];
   Bind_Data &bDType    = bd[1];
   Bind_Data &bCMaxLen  = bd[2];
   Bind_Data &bCType    = bd[3];
   Bind_Data &bAutoInc  = bd[4];
   Bind_Data &bPriKey   = bd[5];
   Bind_Data &bNullable = bd[6];

   while(pp.puller(false))
   {
      std::cout << "<field name=\"" << bName << "\" type=\"" << bDType;

      if (!is_null(bAutoInc))
         std::cout << "\" auto_increment=\"true";
      if (!is_null(bPriKey))
         std::cout << "\" primary_key=\"true";
      if (!is_null(bNullable))
         std::cout << "\" not_null=\"true";
      if (!is_null(bCMaxLen))
         std::cout << "\" length=\"" << bCMaxLen;

      // This must come just before the end because
      // if it's a enum or set type, the values will
      // be rendered as child nodes, which means no
      // attributes can follow it.
      if (is_enum_type(bDType) || is_set_type(bDType))
      {
         std::cout << "\">\n";
         add_value_list(bCType);
         std::cout << "</field>\n";
      }
      else
      {
         if (is_int_type(bDType) && is_unsigned_type(bCType))
            std::cout << "\" unsigned=\"true";

         std::cout << "\" />\n";
      }
   }
}

void print_table_schema(const char *host,
                        const char *user,
                        const char *pass,
                        const char *dbase,
                        const char *tname)
{
   auto f = [&dbase, &tname](MYSQL &mysql)
   {
      MParam params[3] = { dbase, tname };
      std::cout << "<?xml version=\"1.0\" ?>\n<resultset>\n";
      std::cout << "<schema name=\"" << tname << "\">\n";

      execute_query_pull(mysql, print_columns_as_fields, table_schema_query, params);

      std::cout << "</schema>\n";
      std::cout << "</resultset>\n";
   };

   start_mysql(f,host,user,pass,"information_schema");
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

   get_querier_pack(fqp, host, user, password, dbase);
}

void show_usage(void)
{
   std::cout << "Usage instructions:\n\n"
      "xmlify [-u USER] [-pPASSWORD] [-h HOST] [-e SQL_STATEMENT] [-s] [-t tablename] database_name\n\n"
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
      "-t tablename\n"
      "   Output named table's schema.\n"
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
   const char *tablename = nullptr;
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
            case 't':
               tablename = argv[++i];
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
   else if (dbase && tablename)
   {
      print_table_schema(host, user, password, dbase, tablename);
   }
   else
   {
      try
      {
         run_query(query, host, user, password, dbase, include_schema);
      }
      catch(std::exception &e)
      {
         std::cerr << "Error " << e.what() << std::endl;
      }
   }

   return 0;
}
