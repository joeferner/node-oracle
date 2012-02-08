
#ifndef _outparam_h_
#define _outparam_h_

#include <v8.h>
#include <node.h>
#ifndef WIN32
  #include <unistd.h>
#endif
#include "utils.h"

using namespace node;
using namespace v8;

class OutParam : ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Persistent<FunctionTemplate> constructorTemplate;

  OutParam();
  ~OutParam();

private:
};

#endif
