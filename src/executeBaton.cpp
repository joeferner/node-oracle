
#include "executeBaton.h"
#include "outParam.h"
#include "nodeOracleException.h"
#include "connection.h"

ExecuteBaton::ExecuteBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values, v8::Handle<v8::Function>* callback) {
  this->connection = connection;
  this->sql = sql;
  this->callback = Persistent<Function>::New(*callback);
  this->returnParam = NULL;
  CopyValuesToBaton(this, values);
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

  if(error) delete error;
}

double CallDateMethod(v8::Local<v8::Date> date, const char* methodName) {
  Handle<Value> args[0];
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
      v8::String::AsciiValue asciiVal(val);
      value->type = VALUE_TYPE_STRING;
      value->value = new std::string(*asciiVal);
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
      value->value = new oracle::occi::Number(d);
      baton->values.push_back(value);
    }

    // output
    else if(val->IsObject() && val->ToObject()->FindInstanceInPrototypeChain(OutParam::constructorTemplate) != v8::Null()) {
      value->type = VALUE_TYPE_OUTPUT;
      value->value = NULL;
      baton->values.push_back(value);
    }

    // unhandled type
    else {
      std::ostringstream message;
      message << "CopyValuesToBaton: Unhandled value type";
      throw NodeOracleException(message.str());
    }
  }
}
