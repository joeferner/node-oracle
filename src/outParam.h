
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
  static v8::Handle<v8::Value> GetType(const v8::Arguments& args);
  int _type;
  int _size;
  OutParam();
  ~OutParam();
  
  int type();  
  int size();  
  static const int OCCIINT = 0;
  static const int OCCISTRING = 1;

private:
};

#endif
