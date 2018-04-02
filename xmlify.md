# XMLIFY demonstration program

Originally, this program was made to demonstrate the use of *push*
processing of a MySQL query, using the `execute_query` function.

## *PUSH* Processing Example

The `run_query` function demonstrates how to use *push* processing,
where the library *pushes* the data to the application by invoking a
callback function for each record.

## A New and Improved *xmlify*

While it still serves this purpose, *xmlify* now has a new feature
that prints an XML document containing the schema of a MySQL table.
The code for this feature is found in the `print_table_schema`.
This feature uses the more direct `execute_query_pull` function
to read the MySQL records.

## *xmlify* Gets a Mission

The main reason for adding the `print_table_schema` feature is to
support a new project, [gensfw](https://github.com/cjungmann/gensfw),
that uses the XML schema output to generate SQL and SRM scripts for 
the [Schema Framework](https://github.com/cjungmann/schemafw).  Since
the scripts require careful coordination with the MySQL table definitions,
automatic script generation can save time and reduce errors.

