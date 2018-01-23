# Project MySQL Callback (libmysqlcb)

A library that simplifies access to MySQL data.

## Goals

The project has several goals:
- to learn how to design and deliver a Linux Shared-object library,
- to use [Functional Programming](https://en.wikipedia.org/wiki/Functional_programming),
- to exclusively use stack memory (except where other libraries allocate heap memory),
- to design a MySQL library for accessing MySQL data that can be used for a Schema Framework
  utility as well as, eventually, to replace the MySQL code in the Schema Framework
  server program.

### Learning

Although I often use libraries in my work, I don't have a very deep understanding of how
they are designed and set up for inclusion in other projects.  This project has been an
opportunity to practice writing a `configure` script to generate a `Makefile` that 
appropriately builds the project, and provides install and uninstall features.  Designing
and using the library will also provide some practice in designing a useful API.

### Functional Programming

So-called *Functional Programming* seems to be popular now.  I had the impression that
functional programming is the practice of calling functions to run a program, rather than
using objects and methods.  I wrote most of the code with that idea, but then, when I reviewed
the [Wiki page on Functional Programming](https://en.wikipedia.org/wiki/Functional_programming),
I realized that my understanding incorrect.

Nevertheless, many of the techniques I use follow the functional model:
- I avoid side-effects by mostly avoiding global variables, the exceptions being global constant
  data-type manipulator objects that are immutable and contain no data.
- I widely use lambdas and closures.
- The main interface with the library is through higher-order functions that accept wrapped
  functions (lambda or not) as arguments, and through which the result of the function
  is communicated.

There are some efficiency issues that prevent some functional programming techniques.  This is
mainly exhibited my use of a loop rather than recusion to process a query's results.  

### Stack Memory

I use automatic variables and type-casted `alloca` memory for all objects in the program.  Then the
stack-based object or data is ready for use, the program passes it to a continuation function that
is either a well-known function or a function-wrapped argument.  The local data or object is
unwound when the continuation function returns.  This method eliminates the need for destructors.

The advantage of using stack memory is that it prevents memory leaks and fragmentation.
There is no need to search for and keep track of heap memory: the next available memory
is at the top of the stack.  This advantage is particularly useful in a long-running process
such as a FastCGI program that may run for hours at a time, processing thousands of queries.

### Schema Framework

This project is a refinement of the techniques I used to develop the MySQL part of the
Schema Framework.  It reflects some of what I've learned there, making this MySQL library
(hopefully) easier to use and understand.