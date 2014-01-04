
#ifndef _connection_h_
#define _connection_h_

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#ifndef WIN32
  #include <unistd.h>
#endif
#include <occi.h>
#include <oro.h>
#include "utils.h"
#include "executeBaton.h"

using namespace node;
using namespace v8;

// Handle legacy V8 API
#include <uv.h>
#include <node_object_wrap.h>
namespace uni {
#if NODE_MODULE_VERSION >= 0x000D
  typedef void FunctionRetType;
  typedef v8::FunctionCallbackInfo<v8::Value> FunctionArgs;
  typedef v8::Local<Value> BufferType;
# define SCOPE_RETURN(scope, args, res) { args.GetReturnValue().Set(res); return; }
# define THROW(x, m) { ThrowException(x(String::New(m))); return; }
# define HANDLE_SCOPE(scope) HandleScope scope(Isolate::GetCurrent()) 
# define BUFFER_TO_HANDLE(ARG) (ARG)
# define DATE_CAST(d) Local<Date>::Cast(d)
  template <class T>
  void Reset(Persistent<T>& persistent, Handle<T> handle) {
    persistent.Reset(Isolate::GetCurrent(), handle);
  }
  template <class T>
  Handle<T> Deref(Persistent<T>& handle) {
    return Handle<T>::New(Isolate::GetCurrent(), handle);
  }
#else
  typedef Handle<Value> FunctionRetType;
  typedef Arguments FunctionArgs;
  typedef node::Buffer* BufferType;
# define SCOPE_RETURN(scope, args, res) return scope.Close(res)
# define THROW(x, m) return ThrowException(x(String::New(m)))
# define HANDLE_SCOPE(scope) HandleScope scope
# define BUFFER_TO_HANDLE(ARG) (ARG)->handle_
# define DATE_CAST(d) Date::Cast(*(d))
  template <class T>
  void Reset(Persistent<T>& persistent, Handle<T> handle) {
    persistent = Persistent<T>::New(handle);
  }
  template <class T>
  Handle<T> Deref(Persistent<T>& handle) {
    return Local<T>::New(handle);
  }
#endif
}

class Connection : public ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static uni::FunctionRetType New(const uni::FunctionArgs& args);
  static uni::FunctionRetType Execute(const uni::FunctionArgs& args);
  static uni::FunctionRetType ExecuteSync(const uni::FunctionArgs& args);
  static uni::FunctionRetType Close(const uni::FunctionArgs& args);
  static uni::FunctionRetType IsConnected(const uni::FunctionArgs& args);
  static uni::FunctionRetType Commit(const uni::FunctionArgs& args);
  static uni::FunctionRetType Rollback(const uni::FunctionArgs& args);
  static uni::FunctionRetType SetAutoCommit(const uni::FunctionArgs& args);
  static uni::FunctionRetType SetPrefetchRowCount(const uni::FunctionArgs& args);
  static Persistent<FunctionTemplate> constructorTemplate;
  static void EIO_Execute(uv_work_t* req);
  static void EIO_AfterExecute(uv_work_t* req, int status);
  static void EIO_Commit(uv_work_t* req);
  static void EIO_AfterCommit(uv_work_t* req, int status);
  static void EIO_Rollback(uv_work_t* req);
  static void EIO_AfterRollback(uv_work_t* req, int status);
  void closeConnection();

  Connection();
  ~Connection();

  void setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection);
  oracle::occi::Environment* getEnvironment() { return m_environment; }

private:
  static int SetValuesOnStatement(oracle::occi::Statement* stmt, ExecuteBaton* baton);
  static void CreateColumnsFromResultSet(oracle::occi::ResultSet* rs, ExecuteBaton* baton, std::vector<column_t*> &columns);
  static row_t* CreateRowFromCurrentResultSetRow(oracle::occi::ResultSet* rs, ExecuteBaton* baton, std::vector<column_t*> &columns);
  static Local<Array> CreateV8ArrayFromRows(ExecuteBaton* baton, std::vector<column_t*> columns, std::vector<row_t*>* rows);
  static Local<Object> CreateV8ObjectFromRow(ExecuteBaton* baton, std::vector<column_t*> columns, row_t* currentRow);
  static void handleResult(ExecuteBaton* baton, Handle<Value> (&argv)[2]);

  oracle::occi::Connection* m_connection;
  oracle::occi::Environment* m_environment;
  bool m_autoCommit;
  int m_prefetchRowCount;
};

#endif
