
#ifndef _reader_h_
#define _reader_h_


#include <v8.h>
#include <node.h>
#include <occi.h>
#include "readerBaton.h"

using namespace node;
using namespace v8;

class Reader : public ObjectWrap {
public:
  static Persistent<FunctionTemplate> constructorTemplate;
  static void Init(Handle<Object> target);
  static uni::CallbackType New(const uni::FunctionCallbackInfo& args);
  static uni::CallbackType NextRows(const uni::FunctionCallbackInfo& args);
  static void EIO_NextRows(uv_work_t* req);
  static void EIO_AfterNextRows(uv_work_t* req, int status);

  Reader();
  ~Reader();

  void setBaton(ReaderBaton* baton);

private:
  ReaderBaton* m_baton;
};

#endif
