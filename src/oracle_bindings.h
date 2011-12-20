
#ifndef _oracle_binding_h_
#define _oracle_binding_h_

#include <v8.h>
#include <node.h>
#include <unistd.h>
#include <occi.h>
#include "utils.h"

using namespace node;
using namespace v8;

class OracleClient : ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Connect(const Arguments& args);
  static void EIO_Connect(eio_req* req);
  static int EIO_AfterConnect(eio_req* req);

  OracleClient();
  ~OracleClient();

private:
  static Persistent<FunctionTemplate> s_ct;
  oracle::occi::Environment* m_environment;
};

#endif
