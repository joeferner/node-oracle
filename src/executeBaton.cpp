
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
	  case VALUE_TYPE_ARRAY:
	    if (val->value != NULL && val->elemetnsType == oracle::occi::OCCI_SQLT_STR)
		  delete (char*)val->value;
	    else if (val->value != NULL && val->elemetnsType == oracle::occi::OCCI_SQLT_NUM)
          delete (char*)val->value;
		
		if (val->elementLength != NULL)
		  delete val->elementLength;

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

	// array
    else if (val->IsArray()) {
      value->type = VALUE_TYPE_ARRAY;
      Local<Array> arr = Local<Array>::Cast(val);
	  GetVectorParam(baton, value, arr);
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

void ExecuteBaton::GetVectorParam(ExecuteBaton* baton, value_t *value, Local<Array> arr) {
  // In case the array is empty just initialize the fields as we would need something in Connection::SetValuesOnStatement
  if (arr->Length() < 1) {
	value->value = new int[0];
	value->collectionLength = 0;
    value->elementsSize = 0;
    value->elementLength = new ub2[0];
	value->elemetnsType = oracle::occi::OCCIINT;
	return;
  }
  
  // Next we create the array buffer that will be used later as the value for the param (in Connection::SetValuesOnStatement)
  // The array type will be derived from the type of the first element.
  Local<Value> val = arr->Get(0);

  // String array
  if (val->IsString()) {
    value->elemetnsType = oracle::occi::OCCI_SQLT_STR;

    // Find the longest string, this is necessary in order to create a buffer later.
    int longestString = 0;
    for(unsigned int i = 0; i < arr->Length(); i++) {
    Local<Value> currVal = arr->Get(i);
    if (currVal->ToString()->Utf8Length() > longestString)
        longestString = currVal->ToString()->Utf8Length();
    }

	// Add 1 for '\0'
    ++longestString;

	// Create a long char* that will hold the entire array, it is important to create a FIXED SIZE array,
    // meaning all strings have the same allocated length.
    char* strArr = new char[arr->Length() * longestString];
    value->elementLength = new ub2[arr->Length()];

    // loop thru the arr and copy the strings into the strArr
    int bytesWritten = 0;
    for(unsigned int i = 0; i < arr->Length(); i++) {
      Local<Value> currVal = arr->Get(i);
	  if(!currVal->IsString()) {
        std::ostringstream message;
        message << "Input array has object with invalid type at index " << i << ", all object must be of type 'string' which is the type of the first element";
        baton->error = new std::string(message.str());
		return;
	  }
            
	  String::Utf8Value utfStr(currVal);
									
      // Copy this string onto the strArr (we put \0 in the beginning as this is what strcat expects).
      strArr[bytesWritten] = '\0';
      strncat(strArr + bytesWritten, *utfStr, longestString);
      bytesWritten += longestString;
									
      // Set the length of this element, add +1 for the '\0'
      value->elementLength[i] = utfStr.length() + 1;
    }

    value->value = strArr;
    value->collectionLength = arr->Length();
    value->elementsSize = longestString;
  }

  // Integer array.
  else if (val->IsNumber()) {
	value->elemetnsType = oracle::occi::OCCI_SQLT_NUM;
	
	// Allocate memory for the numbers array, Number in Oracle is 21 bytes
    unsigned char* numArr = new unsigned char[arr->Length() * 21];
	value->elementLength = new ub2[arr->Length()];

    for(unsigned int i = 0; i < arr->Length(); i++) {
      Local<Value> currVal = arr->Get(i);
      if(!currVal->IsNumber()) {
        std::ostringstream message;
        message << "Input array has object with invalid type at index " << i << ", all object must be of type 'number' which is the type of the first element";
        baton->error = new std::string(message.str());
		return;
	  }

	  // JS numbers can exceed oracle numbers, make sure this is not the case.
	  double d = currVal->ToNumber()->Value();
	  if (d > 9.99999999999999999999999999999999999999*std::pow(10, 125) || d < -9.99999999999999999999999999999999999999*std::pow(10, 125)) {
        std::ostringstream message;
        message << "Input array has number that is out of the range of Oracle numbers, check the number at index " << i;
        baton->error = new std::string(message.str());
		return;
	  }
	  
	  // Convert the JS number into Oracle Number and get its bytes representation
	  oracle::occi::Number n = d;
      oracle::occi::Bytes b = n.toBytes();
      value->elementLength[i] = b.length ();
      b.getBytes(&numArr[i*21], b.length());
    }

    value->value = numArr;
    value->collectionLength = arr->Length();
    value->elementsSize = 21;
  }

  // Unsupported type
  else {
    baton->error = new std::string("The type of the first element in the input array is not supported");
  }
}