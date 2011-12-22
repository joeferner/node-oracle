
#ifndef _connection_h_
#define _connection_h_

#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <occi.h>
#include <oro.h>
#include "utils.h"
#include "nodeOracleException.h"
#include "executeBaton.h"

using namespace node;
using namespace v8;

class Connection : ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Execute(const Arguments& args);
  static Handle<Value> Close(const Arguments& args);
  static Persistent<FunctionTemplate> constructorTemplate;
  static void EIO_Execute(eio_req* req);
  static int EIO_AfterExecute(eio_req* req);
  void closeConnection();

  Connection();
  ~Connection();

  void setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection);
  oracle::occi::Environment* getEnvironment() { return m_environment; }

private:
  static int SetValuesOnStatement(oracle::occi::Statement* stmt, std::vector<value_t*> &values);
  static void CreateColumnsFromResultSet(oracle::occi::ResultSet* rs, std::vector<column_t*> &columns);
  static row_t* CreateRowFromCurrentResultSetRow(oracle::occi::ResultSet* rs, std::vector<column_t*> &columns);
  static Local<Array> CreateV8ArrayFromRows(ExecuteBaton* baton);
  static Local<Object> CreateV8ObjectFromRow(ExecuteBaton* baton, row_t* currentRow);

  oracle::occi::Connection* m_connection;
  oracle::occi::Environment* m_environment;
};

#endif
