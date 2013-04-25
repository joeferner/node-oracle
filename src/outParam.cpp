
#include "outParam.h"

Persistent<FunctionTemplate> OutParam::constructorTemplate;

void OutParam::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructorTemplate = Persistent<FunctionTemplate>::New(t);
  constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  constructorTemplate->SetClassName(String::NewSymbol("OutParam"));

  t->PrototypeTemplate()->Set(String::NewSymbol("getType"), FunctionTemplate::New(GetType)->GetFunction());
  target->Set(String::NewSymbol("OutParam"), constructorTemplate->GetFunction());
}

Handle<Value> OutParam::New(const Arguments& args) {
  HandleScope scope;

  OutParam *client = new OutParam();
  client->_type = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
  client->_size = args[1]->IsUndefined() ? 200 : args[1]->NumberValue();
  client->Wrap(args.This());
  return args.This();
}

OutParam::OutParam() {
}

OutParam::~OutParam() {
}

Handle<Value> OutParam::GetType(const Arguments& args) {
  HandleScope scope;
  OutParam* obj = ObjectWrap::Unwrap<OutParam>(args.This());

  return scope.Close(Number::New(obj->_type));
}

int OutParam::type() {
  return _type;
}

int OutParam::size() {
  return _size;
}
