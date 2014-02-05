
#include "executeBaton.h"
#include "connection.h"
#include "outParam.h"
#include <iostream>
using namespace std;

ExecuteBaton::ExecuteBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values, v8::Handle<v8::Function>* callback) {
  this->connection = connection;
  this->sql = sql;
  if(callback!=NULL) {
    uni::Reset(this->callback, *callback);
  }
  this->outputs = new std::vector<output_t*>();
  this->error = NULL;
  if (values) CopyValuesToBaton(this, values);
  this->rows = NULL;
}

ExecuteBaton::~ExecuteBaton() {
  callback.Dispose();

  for (std::vector<column_t*>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator) {
    column_t* col = *iterator;
    delete col;
  }

  ResetValues();
  ResetRows();
  ResetOutputs();
  ResetError();
}

void ExecuteBaton::ResetValues() {
  for (std::vector<value_t*>::iterator iterator = values.begin(), end = values.end(); iterator != end; ++iterator) {

    value_t* val = *iterator;
    switch (val->type) {
      case VALUE_TYPE_STRING:
        delete (std::string*)val->value;
        break;
      case VALUE_TYPE_NUMBER:
        delete (oracle::occi::Number*)val->value;
        break;
      case VALUE_TYPE_TIMESTAMP:
        delete (oracle::occi::Timestamp*)val->value;
        break;
    }
    delete val;
  }
  values.clear();
}

void ExecuteBaton::ResetRows() {
  if (rows) {
    for (std::vector<row_t*>::iterator iterator = rows->begin(), end = rows->end(); iterator != end; ++iterator) {
      row_t* currentRow = *iterator;
      delete currentRow;
    }

    delete rows;
    rows = NULL;
  }
}

void ExecuteBaton::ResetOutputs() {
  if (outputs) {
    for (std::vector<output_t*>::iterator iterator = outputs->begin(), end = outputs->end(); iterator != end; ++iterator) {
      output_t* o = *iterator;
      delete o;
    }
    delete outputs;
    outputs = NULL;
  }
}

void ExecuteBaton::ResetError() {
  if (error) {
    delete error;
    error = NULL;
  }
}

double CallDateMethod(v8::Local<v8::Date> date, const char* methodName) {
  Handle<Value> args[1]; // should be zero but on windows the compiler will not allow a zero length array
  Local<Value> result = Local<Function>::Cast(date->Get(String::New(methodName)))->Call(date, 0, args);
  return Local<Number>::Cast(result)->Value();
}

oracle::occi::Timestamp* V8DateToOcciDate(oracle::occi::Environment* env, v8::Local<v8::Date> val) {
  int year = CallDateMethod(val, "getUTCFullYear");
  int month = CallDateMethod(val, "getUTCMonth") + 1;
  int day = CallDateMethod(val, "getUTCDate");
  int hours = CallDateMethod(val, "getUTCHours");
  int minutes = CallDateMethod(val, "getUTCMinutes");
  int seconds = CallDateMethod(val, "getUTCSeconds");
  int fs = CallDateMethod(val, "getUTCMilliseconds") * 1000000; // occi::Timestamp() wants nanoseconds
  oracle::occi::Timestamp* d = new oracle::occi::Timestamp(env, year, month, day, hours, minutes, seconds, fs);
  return d;
}

void ExecuteBaton::CopyValuesToBaton(ExecuteBaton* baton, v8::Local<v8::Array>* values) {
    //XXX cache Length()
  for(uint32_t i=0; i<(*values)->Length(); i++) {
    v8::Local<v8::Value> val = (*values)->Get(i);

    value_t *value = new value_t();

    // null
    if(val->IsNull()) {
      value->type = VALUE_TYPE_NULL;
      value->value = NULL;
      baton->values.push_back(value);
    }

    // string
    else if(val->IsString()) {
      v8::String::Utf8Value utf8Value(val);
      value->type = VALUE_TYPE_STRING;
      value->value = new std::string(*utf8Value);
      baton->values.push_back(value);
    }

    // date
    else if(val->IsDate()) {
      value->type = VALUE_TYPE_TIMESTAMP;
      value->value = V8DateToOcciDate(baton->connection->getEnvironment(), uni::DateCast(val));
      baton->values.push_back(value);
    }

    // number
    else if(val->IsNumber()) {
      value->type = VALUE_TYPE_NUMBER;
      double d = v8::Number::Cast(*val)->Value();
      value->value = new oracle::occi::Number(d); // XXX not deleted in dtor
      baton->values.push_back(value);
    }

    // output
    else if(val->IsObject() && val->ToObject()->FindInstanceInPrototypeChain(uni::Deref(OutParam::constructorTemplate)) != v8::Null()) {
      OutParam* op = node::ObjectWrap::Unwrap<OutParam>(val->ToObject());

      // [rfeng] The OutParam object will be destructed. We need to create a new copy.
      // [bjouhier] new fails with weird error message - fix this later if we really need to copy (do we?)
      OutParam* p = op; //new OutParam(*op);
      value->type = VALUE_TYPE_OUTPUT;
      value->value = p; 
      baton->values.push_back(value);

      output_t* output = new output_t();
      output->rows = NULL;
      output->type = p->type();
      output->index = i + 1;
      baton->outputs->push_back(output);
    }
    // unhandled type
    else {
        //XXX leaks new value on error
      std::ostringstream message;
      message << "CopyValuesToBaton: Unhandled value type: " << (val->IsUndefined() ? "undefined" : "unknown");
      baton->error = new std::string(message.str());
      return;
    }

  }
}
