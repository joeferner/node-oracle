#include <string.h>
#include "connection.h"
#include "executeBaton.h"
#include "reader.h"

using namespace std;

Persistent<FunctionTemplate> Reader::constructorTemplate;

void Reader::Init(Handle<Object> target) {
  UNI_SCOPE(scope);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  uni::Reset(constructorTemplate, t);
  uni::Deref(constructorTemplate)->InstanceTemplate()->SetInternalFieldCount(1);
  uni::Deref(constructorTemplate)->SetClassName(String::NewSymbol("Reader"));

  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "nextRows", NextRows);
  target->Set(String::NewSymbol("Reader"), uni::Deref(constructorTemplate)->GetFunction());
}

Reader::Reader(): ObjectWrap() {
}

Reader::~Reader() {
  delete m_baton;
  m_baton = NULL;
}

uni::CallbackType Reader::New(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);

  Reader* reader = new Reader();
  reader->Wrap(args.This());

  UNI_RETURN(scope, args, args.This());
}

void Reader::setBaton(ReaderBaton* baton) {
  m_baton = baton;
}

uni::CallbackType Reader::NextRows(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Reader* reader = ObjectWrap::Unwrap<Reader>(args.This());
  ReaderBaton* baton = reader->m_baton;
  if (baton->error) {
    Local<String> message = String::New(baton->error->c_str());
    UNI_THROW(Exception::Error(message));
  }

  if (args.Length() > 1) {
    REQ_INT_ARG(0, count);
    REQ_FUN_ARG(1, callback);
    baton->count = count;
    uni::Reset(baton->nextRowsCallback, callback);
  } else {
    REQ_FUN_ARG(0, callback);
    baton->count = baton->connection->getPrefetchRowCount();
    uni::Reset(baton->nextRowsCallback, callback);
  }
  if (baton->count <= 0) baton->count = 1;

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_NextRows, (uv_after_work_cb)EIO_AfterNextRows);
  baton->connection->Ref();

  UNI_RETURN(scope, args, Undefined());
}

void Reader::EIO_NextRows(uv_work_t* req) {
  ReaderBaton* baton = static_cast<ReaderBaton*>(req->data);

  baton->rows = NULL;
  baton->error = NULL;

  try {
    if(! baton->connection->getConnection()) {
      baton->error = new std::string("Connection already closed");
      return;
    }
    if (!baton->rs) {
      baton->stmt = baton->connection->getConnection()->createStatement(baton->sql);
      baton->stmt->setAutoCommit(baton->connection->getAutoCommit());
      baton->stmt->setPrefetchRowCount(baton->count);
      Connection::SetValuesOnStatement(baton->stmt, baton);
      if (baton->error) goto cleanup;

      int status = baton->stmt->execute();
      if(status != oracle::occi::Statement::RESULT_SET_AVAILABLE) {
         baton->error = new std::string("Connection already closed");
        return;
      }     
      baton->rs = baton->stmt->getResultSet();
      Connection::CreateColumnsFromResultSet(baton->rs, baton, baton->columns);
      if (baton->error) goto cleanup;
    }
    baton->rows = new vector<row_t*>();

    for (int i = 0; i < baton->count && baton->rs->next(); i++) {
      row_t* row = Connection::CreateRowFromCurrentResultSetRow(baton->rs, baton, baton->columns);
      if (baton->error) goto cleanup;
      baton->rows->push_back(row);
    }
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new string(ex.getMessage());
  } catch (const exception& ex) {
    baton->error = new string(ex.what());
  } catch (...) {
    baton->error = new string("Unknown Error");
  }
cleanup:
  // nothing for now, cleanup happens in destructor
  ;
}

#if NODE_MODULE_VERSION >= 0x000D
void ReaderWeakReferenceCallback(Isolate* isolate, v8::Persistent<v8::Function>* callback, void* dummy)
{
  callback->Dispose();
}
#else
void ReaderWeakReferenceCallback(v8::Persistent<v8::Value> callback, void* dummy)
{
  (Persistent<Function>(Function::Cast(*callback))).Dispose();
}
#endif

void Reader::EIO_AfterNextRows(uv_work_t* req, int status) {
  UNI_SCOPE(scope);
  ReaderBaton* baton = static_cast<ReaderBaton*>(req->data);

  baton->connection->Unref();

  try {
    Handle<Value> argv[2];
    Connection::handleResult(baton, argv);
    node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->nextRowsCallback), 2, argv);
  } catch(const exception &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.what()));
    argv[1] = Undefined();
    node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->nextRowsCallback), 2, argv);
  }
  baton->nextRowsCallback.MakeWeak((void*)NULL, ReaderWeakReferenceCallback);
}

