
#ifndef _statement_h_
#define _statement_h_


#include <v8.h>
#include <node.h>
#include <occi.h>
#include "statementBaton.h"

using namespace node;
using namespace v8;

class Statement : public ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructorTemplate;
  static void Init(Handle<Object> target);
  static uni::CallbackType New(const uni::FunctionCallbackInfo& args);
  static uni::CallbackType Execute(const uni::FunctionCallbackInfo& args);
  static void EIO_Execute(uv_work_t* req);
  static void EIO_AfterExecute(uv_work_t* req, int status);

  Statement();
  ~Statement();

  void setBaton(StatementBaton* baton);

private:
  StatementBaton* m_baton;
};

#endif
