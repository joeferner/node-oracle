
#include "oracle_bindings.h"
#include "connection.h"
#include "outParam.h"

Persistent<FunctionTemplate> OracleClient::s_ct;

struct connect_baton_t {
  OracleClient* client;
  Persistent<Function> callback;
  std::string hostname;
  std::string user;
  std::string password;
  std::string database;
  uint32_t port;
  oracle::occi::Environment *environment;

  std::string* error;
  oracle::occi::Connection* connection;
};

void OracleClient::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  s_ct = Persistent<FunctionTemplate>::New(t);
  s_ct->InstanceTemplate()->SetInternalFieldCount(1);
  s_ct->SetClassName(String::NewSymbol("OracleClient"));

  NODE_SET_PROTOTYPE_METHOD(s_ct, "connect", Connect);

  target->Set(String::NewSymbol("OracleClient"), s_ct->GetFunction());
}

Handle<Value> OracleClient::New(const Arguments& args) {
  HandleScope scope;

  OracleClient *client = new OracleClient();
  client->Wrap(args.This());
  return args.This();
}

OracleClient::OracleClient() {
  m_environment = oracle::occi::Environment::createEnvironment(oracle::occi::Environment::THREADED_MUTEXED);
}

OracleClient::~OracleClient() {
  oracle::occi::Environment::terminateEnvironment(m_environment);
}

Handle<Value> OracleClient::Connect(const Arguments& args) {
  HandleScope scope;

  REQ_OBJECT_ARG(0, settings);
  REQ_FUN_ARG(1, callback);

  OracleClient* client = ObjectWrap::Unwrap<OracleClient>(args.This());
  connect_baton_t* baton = new connect_baton_t();
  baton->client = client;
  baton->callback = Persistent<Function>::New(callback);
  baton->environment = client->m_environment;

  OBJ_GET_STRING(settings, "hostname", baton->hostname);
  OBJ_GET_STRING(settings, "user", baton->user);
  OBJ_GET_STRING(settings, "password", baton->password);
  OBJ_GET_STRING(settings, "database", baton->database);
  OBJ_GET_NUMBER(settings, "port", baton->port, 1521);

  client->Ref();

  eio_custom(EIO_Connect, EIO_PRI_DEFAULT, EIO_AfterConnect, baton);
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}

void OracleClient::EIO_Connect(eio_req* req) {
  connect_baton_t* baton = static_cast<connect_baton_t*>(req->data);

  baton->error = NULL;

  try {
    std::ostringstream connectionStr;
    connectionStr << "//" << baton->hostname << ":" << baton->port << "/" << baton->database;
    baton->connection = baton->environment->createConnection(baton->user, baton->password, connectionStr.str());
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new std::string(ex.getMessage());
  }
}

int OracleClient::EIO_AfterConnect(eio_req* req) {
  HandleScope scope;
  connect_baton_t* baton = static_cast<connect_baton_t*>(req->data);
  ev_unref(EV_DEFAULT_UC);
  baton->client->Unref();

  Handle<Value> argv[2];
  if(baton->error) {
    argv[0] = Exception::Error(String::New(baton->error->c_str()));
    argv[1] = Undefined();
  } else {
    argv[0] = Undefined();
    Handle<Object> connection = Connection::constructorTemplate->GetFunction()->NewInstance();
    (node::ObjectWrap::Unwrap<Connection>(connection))->setConnection(baton->client->m_environment, baton->connection);
    argv[1] = connection;
  }

  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  baton->callback.Dispose();
  if(baton->error) delete baton->error;

  delete baton;
  return 0;
}

extern "C" {
  static void init(Handle<Object> target) {
    OracleClient::Init(target);
    Connection::Init(target);
    OutParam::Init(target);
  }

  NODE_MODULE(oracle_bindings, init);
}
