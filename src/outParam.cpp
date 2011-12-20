
#include "outParam.h"

Persistent<FunctionTemplate> OutParam::constructorTemplate;

void OutParam::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructorTemplate = Persistent<FunctionTemplate>::New(t);
  constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  constructorTemplate->SetClassName(String::NewSymbol("OutParam"));

  target->Set(String::NewSymbol("OutParam"), constructorTemplate->GetFunction());
}

Handle<Value> OutParam::New(const Arguments& args) {
  HandleScope scope;

  OutParam *client = new OutParam();
  client->Wrap(args.This());
  return args.This();
}

OutParam::OutParam() {
}

OutParam::~OutParam() {
}
