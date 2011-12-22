
#ifndef _excute_baton_h_
#define _excute_baton_h_

class Connection;

#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <occi.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>

enum {
  VALUE_TYPE_NULL = 1,
  VALUE_TYPE_OUTPUT = 2,
  VALUE_TYPE_STRING = 3,
  VALUE_TYPE_NUMBER = 4,
  VALUE_TYPE_DATE = 5,
  VALUE_TYPE_TIMESTAMP = 6,
  VALUE_TYPE_CLOB = 7,
  VALUE_TYPE_BLOB = 8
};

struct column_t {
  int type;
  std::string name;
};

struct row_t {
  std::vector<void*> values;
};

struct value_t {
  int type;
  void* value;
};

class ExecuteBaton {
public:
  ExecuteBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values, v8::Handle<v8::Function>* callback);
  ~ExecuteBaton();

  Connection *connection;
  v8::Persistent<v8::Function> callback;
  std::vector<value_t*> values;
  std::string sql;
  std::vector<column_t*> columns;
  std::vector<row_t*>* rows;
  std::string* error;
  int updateCount;
  int* returnParam;

private:
  static void CopyValuesToBaton(ExecuteBaton* baton, v8::Local<v8::Array>* values);
};

#endif
