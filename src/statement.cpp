#include <string.h>
#include "connection.h"
#include "executeBaton.h"
#include "statement.h"

using namespace std;

Persistent<FunctionTemplate> Statement::constructorTemplate;

void Statement::Init(Handle<Object> target) {
  UNI_SCOPE(scope);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  uni::Reset(constructorTemplate, t);
  uni::Deref(constructorTemplate)->InstanceTemplate()->SetInternalFieldCount(1);
  uni::Deref(constructorTemplate)->SetClassName(String::NewSymbol("Statement"));

  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "execute", Execute);
  target->Set(String::NewSymbol("Statement"), uni::Deref(constructorTemplate)->GetFunction());
}

Statement::Statement(): ObjectWrap() {
}

Statement::~Statement() {
  delete m_baton;
  m_baton = NULL;
}

uni::CallbackType Statement::New(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);

  Statement* statement = new Statement();
  statement->Wrap(args.This());

  UNI_RETURN(scope, args, args.This());
}

void Statement::setBaton(StatementBaton* baton) {
  m_baton = baton;
}

uni::CallbackType Statement::Execute(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Statement* statement = ObjectWrap::Unwrap<Statement>(args.This());
  StatementBaton* baton = statement->m_baton;

  REQ_ARRAY_ARG(0, values);
  REQ_FUN_ARG(1, callback);
  uni::Reset(baton->callback, callback);

  ExecuteBaton::CopyValuesToBaton(baton, &values);
  if (baton->error) {
    Local<String> message = String::New(baton->error->c_str());
    UNI_THROW(Exception::Error(message));
  }

  if (baton->busy) {
    UNI_THROW(Exception::Error(String::New("invalid state: statement is busy with another execute call")));
  }
  baton->busy = true;

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Execute, (uv_after_work_cb)EIO_AfterExecute);
  baton->connection->Ref();

  UNI_RETURN(scope, args, Undefined());
}

void Statement::EIO_Execute(uv_work_t* req) {
  StatementBaton* baton = static_cast<StatementBaton*>(req->data);

  if (!baton->connection->getConnection()) {
    baton->error = new std::string("Connection already closed");
    return;
  }
  if (!baton->stmt) {
    baton->stmt = Connection::CreateStatement(baton);
    if (baton->error) return;
  }
  Connection::ExecuteStatement(baton, baton->stmt);
}

void Statement::EIO_AfterExecute(uv_work_t* req, int status) {
  UNI_SCOPE(scope);
  StatementBaton* baton = static_cast<StatementBaton*>(req->data);

  baton->busy = false;
  baton->connection->Unref();
  // transfer callback to local and dispose persistent handle
  Local<Function> cb = uni::HandleToLocal(uni::Deref(baton->callback));
  baton->callback.Dispose();
  baton->callback.Clear();

  Handle<Value> argv[2];
  Connection::handleResult(baton, argv);

  baton->ResetValues();
  baton->ResetRows();
  baton->ResetOutputs();
  baton->ResetError();
  delete req;

  // invoke callback at the very end because callback may re-enter execute.
  node::MakeCallback(Context::GetCurrent()->Global(), cb, 2, argv);
}

