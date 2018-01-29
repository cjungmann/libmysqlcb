#include <mysql.h>
#include <iostream>

#include "mysqlcb.hpp"

#define CLI "\033["

void clear_screen(void) { std::cout << CLI << "2J" << CLI << "H"; }

const char q_dbases[] = "SELECT SCHEMA_NAME FROM SCHEMATA";
const char q_tables[] = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE"
                                                             "  FROM COLUMNS"
                                                             " WHERE TABLE_SCHEMA=?";

struct Llist
{
   const char   *line;
   unsigned int position;
   Llist        *next;
};

using Llist_Callback = IQuery_Callback<Llist>;
template <typename Func>
using Llist_User = Query_User<Llist, Func>;

void display_list(const Llist *ll, int selection)
{
   clear_screen();
   const Llist *ptr = ll;
   int newposition;
   while(ptr)
   {
      std::cout << ptr->position << " " << ptr->line << std::endl;
      ptr = ptr->next;
   }
   std::cout << "\nEnter a number to select a line: ";
   std::cin >> newposition;
}

void query_list(PullPack &pp)
{
   Llist *head = nullptr;
   Llist *tail = nullptr;

   // We'll just hold on to the first bind_data.
   Bind_Data *bd = pp.binder.bind_data;
   size_t bufflen;
   void *buffer = nullptr;
   int position = 0;

   while (pp.puller(false))
   {
      // Copy string to stack memory
      bufflen = get_size(bd);
      buffer = alloca(bufflen);
      set_with_value(bd, buffer, bufflen);

      // New stack-based list element:
      Llist *llnew = static_cast<Llist*>(alloca(sizeof(Llist)));
      llnew->line = static_cast<const char*>(buffer);
      llnew->position = ++position;
      llnew->next = nullptr;

      // Attach new element to chain:
      if (!tail)
      {
         head = tail = llnew;
      }
      else
      {
         tail->next = llnew;
         tail = llnew;
      }
   }

   display_list(head,1);
}

void use_mysql(MYSQL &mysql, const char *dbname)
{
   if (dbname)
   {
      std::cout << "This ain't done yet.\n";
   }
   else
   {
      execute_query_pull(mysql, query_list, q_dbases);
   }
}

void open_mysql(const char *host,
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
                                         host, user, pass, "information_schema",
                                         port, socket, client_flag);

      if (handle)
      {
         use_mysql(mysql, dbase);
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

void show_usage(void)
{
   std::cout << "Usage instructions:\n\n"
      "sqldrill [-u USER] [-pPASSWORD] [-h HOST] [database_name]\n\n"
      "Options:\n"
      "-h [host]\n"
      "   MYSQL host to which the connection is to be made.\n"
      "   If this option is omitted, localhost will be used.\n"
      "--help\n"
      "   This usage display.\n"
      "-p[password]\n"
      "   Password for user account.  Note there is no space between -p and the password value.\n"
      "-u [user]\n"
      "   User account to use for the connection.\n"
      "\n"
      "The utility will use default values under [client] in ~/.my.cnf for unspecified options.\n"
      "\n"
      "If a database is not specified, sqldrill will begin with a page showing a list of databases.\n";
}

int main(int argc, char **argv)
{
   const char *dbase = nullptr;
   const char *host = "localhost";
   const char *user = nullptr;
   const char *password = nullptr;
   bool display_usage = 0;

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
      open_mysql(host, user, password, dbase);

   return 0;
}


