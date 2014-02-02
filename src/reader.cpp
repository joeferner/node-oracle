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
  if (baton->busy) {
    UNI_THROW(Exception::Error(String::New("invalid state: reader is busy with another nextRows call")));
  }
  baton->busy = true;

  if (args.Length() > 1) {
    REQ_INT_ARG(0, count);
    REQ_FUN_ARG(1, callback);
    baton->count = count;
    uni::Reset(baton->callback, callback);
  } else {
    REQ_FUN_ARG(0, callback);
    baton->count = baton->connection->getPrefetchRowCount();
    uni::Reset(baton->callback, callback);
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

  baton->rows = new vector<row_t*>();
  if (baton->done) return;

  if (!baton->connection->getConnection()) {
    baton->error = new std::string("Connection already closed");
    return;
  }
  if (!baton->rs) {
    try {
      baton->stmt = baton->connection->getConnection()->createStatement(baton->sql);
      baton->stmt->setAutoCommit(baton->connection->getAutoCommit());
      baton->stmt->setPrefetchRowCount(baton->count);
      Connection::SetValuesOnStatement(baton->stmt, baton);
      if (baton->error) return;

      int status = baton->stmt->execute();
      if (status != oracle::occi::Statement::RESULT_SET_AVAILABLE) {
         baton->error = new std::string("Connection already closed");
        return;
      }     
      baton->rs = baton->stmt->getResultSet();
    } catch (oracle::occi::SQLException &ex) {
      baton->error = new string(ex.getMessage());
      return;
    }
    Connection::CreateColumnsFromResultSet(baton->rs, baton, baton->columns);
    if (baton->error) return;
  }
  for (int i = 0; i < baton->count && baton->rs->next(); i++) {
    row_t* row = Connection::CreateRowFromCurrentResultSetRow(baton->rs, baton, baton->columns);
    if (baton->error) return;
    baton->rows->push_back(row);
  }
  if (baton->rows->size() < (size_t)baton->count) baton->done = true;
}

void Reader::EIO_AfterNextRows(uv_work_t* req, int status) {
  UNI_SCOPE(scope);
  ReaderBaton* baton = static_cast<ReaderBaton*>(req->data);

  baton->busy = false;
  baton->connection->Unref();
  // transfer callback to local and dispose persistent handle
  Local<Function> cb = uni::HandleToLocal(uni::Deref(baton->callback));
  baton->callback.Dispose();
  baton->callback.Clear();

  Handle<Value> argv[2];
  Connection::handleResult(baton, argv);

  baton->ResetRows();
  if (baton->done || baton->error) {
    // free occi resources so that we don't run out of cursors if gc is not fast enough
    // reader destructor will delete the baton and everything else.
    baton->ResetStatement();
  }
  delete req;

  // invoke callback at the very end because callback may re-enter nextRows.
  node::MakeCallback(Context::GetCurrent()->Global(), cb, 2, argv);
}

