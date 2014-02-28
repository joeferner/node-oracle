#include "connection.h" // to get compat macros
#include "outParam.h"
#include <iostream>
using namespace std;

Persistent<FunctionTemplate> OutParam::constructorTemplate;

/**
 * function OutParam(type, options) {
 *   this._type = type || 0;
 *   this._size = options.size;
 *   this._inOut.hasInParam = options.in;
 * }
 */
void OutParam::Init(Handle<Object> target) {
  UNI_SCOPE(scope);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  uni::Reset(constructorTemplate, t);
  uni::Deref(constructorTemplate)->InstanceTemplate()->SetInternalFieldCount(1);
  uni::Deref(constructorTemplate)->SetClassName(String::NewSymbol("OutParam"));
  target->Set(String::NewSymbol("OutParam"), uni::Deref(constructorTemplate)->GetFunction());
}

uni::CallbackType OutParam::New(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  OutParam *outParam = new OutParam();

  if(args.Length() >=1 ) {
    outParam->_type = args[0]->IsUndefined() ? OutParam::OCCIINT : args[0]->NumberValue();
  } else {
    outParam->_type = OutParam::OCCIINT;
  }
  
  if (args.Length() >=2 && !args[1]->IsUndefined()) {
    REQ_OBJECT_ARG(1, opts);
    OBJ_GET_NUMBER(opts, "size", outParam->_size, 200);

    // check if there's an 'in' param
    if (opts->Has(String::New("in"))) {
      outParam->_inOut.hasInParam = true;
      switch(outParam->_type) {
      case OutParam::OCCIINT: {
        OBJ_GET_NUMBER(opts, "in", outParam->_inOut.intVal, 0);
        break;
      }
      case OutParam::OCCIDOUBLE: {
        OBJ_GET_NUMBER(opts, "in", outParam->_inOut.doubleVal, 0);
        break;
      }
      case OutParam::OCCIFLOAT: {
        OBJ_GET_NUMBER(opts, "in", outParam->_inOut.floatVal, 0);
        break;
      }
      case OutParam::OCCINUMBER: {
        OBJ_GET_NUMBER(opts, "in", outParam->_inOut.numberVal, 0);
        break;
      }
      case OutParam::OCCISTRING: {
        OBJ_GET_STRING(opts, "in", outParam->_inOut.stringVal);
        break;
      }
      case OutParam::OCCIVECTOR: {
        // Get the vector's elements type
        Local<Value> valType = opts->Get(String::New("type"));
        if(valType->IsUndefined() || valType->IsNull())
          UNI_THROW(Exception::Error(String::New("OutParam::OCCIVECTOR missing `type` property")));

        // Create the collection according to the elem type.
        int type = valType->ToInt32()->Value();
        switch(type) {
        // Here we create the array buffer that will be used later as the value for the param on the sp statement (Connection::SetValuesOnStatement)
        case OutParam::OCCISTRING: {
          outParam->_inOut.elemetnsType = oracle::occi::OCCI_SQLT_STR;

          // Get the array
          Local<Array> arr = Local<Array>::Cast(opts->Get(String::New("in")));
							
          // Find the longest string, this is necessary in order to create a buffer later.
		  // If "size" was provided by the user we use it as the size of the "longest string", otherwise we would look for it.
          int longestString = 0;
		  if (opts->Has(String::New("in"))) {
		    longestString = outParam->_size;
		  } else {
            for(unsigned int i = 0; i < arr->Length(); i++) {
              Local<Value> val = arr->Get(i);
              if (val->ToString()->Utf8Length() > longestString)
                longestString = val->ToString()->Utf8Length();
            }
		  }

		  // Add 1 for '\0'
          ++longestString;

		  // Create a long char* that will hold the entire array, it is important to create a FIXED SIZE array,
          // meaning all strings have the same allocated length.
          char* strArr = new char[arr->Length() * longestString];
          outParam->_inOut.elementLength = new ub2[arr->Length()];

          // loop thru the arr and copy the strings into the strArr
          int bytesWritten = 0;
          for(unsigned int i = 0; i < arr->Length(); i++) {
            Local<Value> val = arr->Get(i);
            if(!val->IsString()) {
              UNI_THROW(Exception::Error(String::New("OutParam::OCCIVECTOR of type OCCISTRING must include string types only")));
			}
            
			String::Utf8Value utfStr(val);
									
            // Copy this string onto the strArr (we put \0 in the beginning as this is what strcat expects).
            strArr[bytesWritten] = '\0';
            strncat(strArr + bytesWritten, *utfStr, longestString);
            bytesWritten += longestString;
									
            // Set the length of this element, add +1 for the '\0'
            outParam->_inOut.elementLength[i] = utfStr.length() + 1;
          }

          outParam->_inOut.collectionValues = strArr;
          outParam->_inOut.collectionLength = arr->Length();
          outParam->_inOut.elementsSize = longestString;
          break;
        }
        case OutParam::OCCIINT: {
          Local<Array> arr = Local<Array>::Cast( opts->Get(String::New("in")));

          // Allocate memory and copy the ints.
          int* intArr = new int[arr->Length()];
          for(unsigned int i = 0; i < arr->Length(); i++) {
            Local<Value> val = arr->Get(i);
            if(!val->IsInt32()) {
              UNI_THROW(Exception::Error(String::New("OutParam::OCCIVECTOR of type OCCIINT must include integer types only")));
			}

			intArr[i] = val->ToInt32()->Value();
          }

          outParam->_inOut.collectionValues = intArr;
          outParam->_inOut.collectionLength = arr->Length();
          outParam->_inOut.elementsSize = sizeof(int);
          outParam->_inOut.elemetnsType = oracle::occi::OCCIINT;
          break;
        }
        default: 
          UNI_THROW(Exception::Error(String::New("OutParam::OCCIVECTOR unsupported `type`, only supports OCCIINT and OCCISTRING")));
        }
		// End case:OCCIVECTOR
        break;
      }
      default:
        UNI_THROW(Exception::Error(String::New("Unhandled OutPram type!")));
      }
    }
  }
  outParam->Wrap(args.This());
  UNI_RETURN(scope, args, args.This());
}

OutParam::OutParam() {
  _type = OutParam::OCCIINT;
  _inOut.hasInParam = false;
  _size = 200;
}

OutParam::~OutParam() {
  if (_inOut.collectionValues != NULL) {
    // This can be either a char* or int*
    if (_inOut.elemetnsType == oracle::occi::OCCIINT)
      delete (int*)(_inOut.collectionValues);
    else if (_inOut.elemetnsType == oracle::occi::OCCI_SQLT_STR)
      delete (char*)(_inOut.collectionValues);
  }

  if (_inOut.elementLength != 0)
    delete _inOut.elementLength;

  if (_inOut.stringVal != NULL)
    delete _inOut.stringVal;
}

int OutParam::type() {
  return _type;
}

int OutParam::size() {
  return _size;
}
