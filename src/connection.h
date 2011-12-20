
#ifndef _connection_h_
#define _connection_h_

#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <occi.h>
#include "utils.h"

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

private:
  oracle::occi::Connection* m_connection;
  oracle::occi::Environment* m_environment;
};

#endif
