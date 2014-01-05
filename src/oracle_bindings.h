
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

class OracleClient : public ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static uni::CallbackType New(const uni::FunctionCallbackInfo& args);
  static uni::CallbackType Connect(const uni::FunctionCallbackInfo& args);
  static uni::CallbackType ConnectSync(const uni::FunctionCallbackInfo& args);
  static void EIO_Connect(uv_work_t* req);
  static void EIO_AfterConnect(uv_work_t* req, int status);

  OracleClient();
  ~OracleClient();

private:
  static Persistent<FunctionTemplate> s_ct;
  oracle::occi::Environment* m_environment;
  /*
  oracle::occi::ConnectionPool *m_connectionPool;
  */
};

class ConnectBaton {
public:
  ConnectBaton(OracleClient* client, oracle::occi::Environment* environment, v8::Handle<v8::Function>* callback);
  ~ConnectBaton();

  OracleClient* client;
  Persistent<Function> callback;

  std::string hostname;
  std::string user;
  std::string password;
  std::string database;
  std::string tns;
  uint32_t port;

  oracle::occi::Environment* environment;
  oracle::occi::Connection* connection;

  std::string* error;
};


#endif
