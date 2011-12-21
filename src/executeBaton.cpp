
#include "executeBaton.h"
#include "outParam.h"
#include "nodeOracleException.h"

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

void ExecuteBaton::CopyValuesToBaton(ExecuteBaton* baton, v8::Local<v8::Array>* values) {
  for(uint32_t i=0; i<(*values)->Length(); i++) {
    v8::Local<v8::Value> val = (*values)->Get(i);

    value_t *value = new value_t();
    if(val->IsString()) {
      v8::String::AsciiValue asciiVal(val);
      value->type = VALUE_TYPE_STRING;
      value->value = new std::string(*asciiVal);
      baton->values.push_back(value);
    } else if(val->IsObject() && val->ToObject()->FindInstanceInPrototypeChain(OutParam::constructorTemplate) != v8::Null()) {
      value->type = VALUE_TYPE_OUTPUT;
      value->value = NULL;
      baton->values.push_back(value);
    } else {
      std::ostringstream message;
      message << "Unhandled value type";
      throw NodeOracleException(message.str());
    }
  }
}
