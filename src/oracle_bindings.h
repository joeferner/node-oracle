
#ifndef _oracle_binding_h_
#define _oracle_binding_h_

#include <v8.h>
#include <node.h>
#ifndef WIN32
  #include <unistd.h>
#endif
#include <occi.h>
#include "utils.h"

using namespace node;
using namespace v8;

class OracleClient : ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Connect(const Arguments& args);
  static void EIO_Connect(uv_work_t* req);
  static void EIO_AfterConnect(uv_work_t* req, int status);

  OracleClient();
  ~OracleClient();

private:
  static Persistent<FunctionTemplate> s_ct;
  oracle::occi::Environment* m_environment;
};

#endif
