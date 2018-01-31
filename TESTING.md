# mysqlcb Testing

This page will record several tests used to develop and confirm
procedures in support of the `libmysqlcb` library.

## Data Type Output

While numeric data types are straight-forward, mapping directly
to OS and C/C++ language data types (except for MEDIUMINT, the
24-bit integer), other data types present some complications.

### Truncated Strings and Blobs

Strings and blobs can be arbitrarily long.  In the interest of
keeping the memory requirements of the library within reasonable
limits, there is a maximum buffer size for a string or blob. 
Initially, at least, the buffer is limited to 1024 bytes.

Some testing should be done to use truncated strings.

### Date and Time Data Types

The internal storage of these types is unusual.
[MySQL Date and Time Representation](https://dev.mysql.com/doc/internals/en/date-and-time-data-type-representation.html)
shows the memory configuration for the date/time types.

### Enums and Sets

These data types define a set of values from which a field's
value is taken.  An **enum** value selects one from a list, and
a **set** value selects from 0 to all of the set values.

There are some system tables that include these field types.

#### Enum Examples

To find tables in which a *enum* field is used:
~~~sql
SELECT TABLE_SCHEMA,
       TABLE_NAME,
       DATA_TYPE,
       COLUMN_NAME
  FROM information_schema.COLUMNS
 WHERE DATA_TYPE='enum'
~~~


#### Set Examples

To find tables in which a *set* field is used:
~~~sql
SELECT TABLE_SCHEMA,
       TABLE_NAME,
       DATA_TYPE,
       COLUMN_NAME
  FROM information_schema.COLUMNS
 WHERE DATA_TYPE='set'
~~~

This statement should show a small number of *set* examples:
`SELECT sql_mode FROM mysql.event;`

This statement will show a larger number of *set* examples:
`SELECT sql_mode FROM mysql.proc;`