
#include "executeBaton.h"
#include "outParam.h"
#include "nodeOracleException.h"
#include "connection.h"
#include <iostream>
using namespace std;

ExecuteBaton::ExecuteBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values, v8::Handle<v8::Function>* callback) {
  this->connection = connection;
  this->sql = sql;
  if(callback!=NULL) {
    this->callback = Persistent<Function>::New(*callback);
  }
  this->outputs = new std::vector<output_t*>();
  CopyValuesToBaton(this, values);
  this->error = NULL;
}

ExecuteBaton::~ExecuteBaton() {
  callback.Dispose();

  for (std::vector<column_t*>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator) {
    column_t* col = *iterator;
    delete col;
  }

  for (std::vector<value_t*>::iterator iterator = values.begin(), end = values.end(); iterator != end; ++iterator) {

    value_t* val = *iterator;
    if(val->type == VALUE_TYPE_STRING) {
      delete (std::string*)val->value;
    }
    delete val;
  }

  if(rows) {
    for (std::vector<row_t*>::iterator iterator = rows->begin(), end = rows->end(); iterator != end; ++iterator) {
      row_t* currentRow = *iterator;
      delete currentRow;
    }

    delete rows;
  }

  if(outputs) {
    for (std::vector<output_t*>::iterator iterator = outputs->begin(), end = outputs->end(); iterator != end; ++iterator) {
      output_t* o = *iterator;
      delete o;
    }
    delete outputs;
  }

  if(error) delete error;
}

double CallDateMethod(v8::Local<v8::Date> date, const char* methodName) {
  Handle<Value> args[1]; // should be zero but on windows the compiler will not allow a zero length array
  Local<Value> result = Local<Function>::Cast(date->Get(String::New(methodName)))->Call(date, 0, args);
  return Local<Number>::Cast(result)->Value();
}

oracle::occi::Date* V8DateToOcciDate(oracle::occi::Environment* env, v8::Local<v8::Date> val) {
  int year = CallDateMethod(val, "getFullYear");
  int month = CallDateMethod(val, "getMonth") + 1;
  int day = CallDateMethod(val, "getDate");
  int hours = CallDateMethod(val, "getHours");
  int minutes = CallDateMethod(val, "getMinutes");
  int seconds = CallDateMethod(val, "getSeconds");
  oracle::occi::Date* d = new oracle::occi::Date(env, year, month, day, hours, minutes, seconds);
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
      value->type = VALUE_TYPE_DATE;
      value->value = V8DateToOcciDate(baton->connection->getEnvironment(), v8::Date::Cast(*val));
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
    else if(val->IsObject() && val->ToObject()->FindInstanceInPrototypeChain(OutParam::constructorTemplate) != v8::Null()) {
      OutParam* op = node::ObjectWrap::Unwrap<OutParam>(val->ToObject());

      // [rfeng] The OutParam object will be destructed. We need to create a new copy.
      OutParam* p = new OutParam(*op);
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
      message << "CopyValuesToBaton: Unhandled value type";
      throw NodeOracleException(message.str());
    }

  }
}
