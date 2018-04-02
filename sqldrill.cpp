#include <mysql.h>
#include <iostream>
#include <exception>

#include "mysqlcb.hpp"

using namespace mysqlcb;

const char CLI[] = "\033[";


const char q_dbases[] = "SELECT SCHEMA_NAME FROM SCHEMATA";
const char q_tables[] = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_SCHEMA=?";
const char q_columns[] =
   "SELECT COLUMN_NAME, COLUMN_TYPE"
   "  FROM COLUMNS"
   " WHERE TABLE_SCHEMA=?"
   "   AND TABLE_NAME=?";

void clear_screen(void) { std::cout << CLI << "2J" << CLI << "H"; }

struct Llist
{
   const char *line;
   uint32_t   position;
   Llist      *next;
};

using ILlist_Callback = IGeneric_Callback<Llist>;
template <typename Func>
using Llist_User = Generic_User<Llist, Func>;

const char *get_selected_line(const Llist &ll, uint32_t selection)
{
   const Llist *ptr = &ll;
   while (ptr)
   {
      if (ptr->position==selection)
         return ptr->line;
      ptr = ptr->next;
   }
   return nullptr;
}


void show_columns(const PullPack &pp)
{
   clear_screen();

   // We'll just hold on to the first bind_data.
   Bind_Data *bd = pp.binder.bind_data;

   while (pp.puller(false))
   {
      Bind_Data *col = bd;
      while(valid(col))
      {
         std::cout << col << " ";
         ++col;
      }
      std::cout << std::endl;
   }
}

void summon_list(ILlist_Callback &cb, const PullPack &pp)
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
      bufflen = get_data_len(bd);
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

   cb(*head);

}

/**
 * This function simply displays each line prefixed with its position value.
 */
void display_list(const Llist &ll, int selection)
{
   clear_screen();
   const Llist *ptr = &ll;
   while(ptr)
   {
      std::cout << ptr->position << " " << ptr->line << std::endl;
      ptr = ptr->next;
   }
   std::cout << "\nEnter a number (0 to return) to select a line: ";
}

/**
 * This function displays a list, gets a response, and returns the selected string.
 */
const char *select_from_list(const Llist &ll)
{
   display_list(ll, 1);

   uint32_t selection;
   std::cin >> selection;
   if (selection)
      return get_selected_line(ll, selection);
   else
      return nullptr;
}

/** */
void show_rows(MYSQL &mysql, const char *dbname, const char *tablename)
{
   auto f_pullpack = [](const PullPack &pp)
   {
      show_columns(pp);
      std::cout << "Press any key to return\n";
      int ival;
      std::cin >> ival;
      
   };
   PullPack_User<decltype(f_pullpack)> cb_pullpack(f_pullpack);

   MParam params[3] = { dbname, tablename };
   execute_query_pull(mysql, cb_pullpack, q_columns, params);
}

/** */
void show_tables(MYSQL &mysql, const char *dbname)
{
   auto f_llist = [&mysql, &dbname](const Llist &ll)
   {
      const char *rline;
      while((rline=select_from_list(ll)))
         show_rows(mysql, dbname, rline);
   };
   Llist_User<decltype(f_llist)> cb_llist(f_llist);

   auto f_pullpack = [&mysql, &cb_llist](const PullPack &pp)
   {
      summon_list(cb_llist, pp);
   };
   PullPack_User<decltype(f_pullpack)> cb_pullpack(f_pullpack);

   MParam params[2] = { dbname };
   execute_query_pull(mysql, cb_pullpack, q_tables, params);
}

/** */
void show_dbases(MYSQL &mysql)
{
   auto f_llist = [&mysql](const Llist &ll)
   {
      const char *rline;
      while((rline=select_from_list(ll)))
         show_tables(mysql, rline);
   };
   Llist_User<decltype(f_llist)> cb_llist(f_llist);

   auto f_pullpack = [&mysql, &cb_llist](const PullPack &pp)
   {
      summon_list(cb_llist, pp);
   };
   PullPack_User<decltype(f_pullpack)> cb_pullpack(f_pullpack);
   execute_query_pull(mysql, cb_pullpack, q_dbases);
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
         try
         {
            if (dbase)
               show_tables(mysql, dbase);
            else
               show_dbases(mysql);
         }
         catch(std::exception &e)
         {
            std::cerr << "Caught exception " << e.what() << std::endl;
         }

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


