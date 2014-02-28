
#ifndef _outparam_h_
#define _outparam_h_

#include <v8.h>
#include <node.h>
#ifndef WIN32
  #include <unistd.h>
#endif
#include "utils.h"
#include <occi.h>

using namespace node;
using namespace v8;

struct inout_t {
  bool hasInParam;
  const char* stringVal; 
  int intVal;
  double doubleVal;
  float floatVal;
  oracle::occi::Date dateVal;
  oracle::occi::Timestamp timestampVal;
  oracle::occi::Number numberVal;

  // This will hold the info needed for binding vectors values
  void* collectionValues;
  ub4 collectionLength;
  sb4 elementsSize; // The size of each element in the array
  ub2* elementLength; //  An array that holds the actual length of each element in the array (in case of strings)
  oracle::occi::Type elemetnsType;
};

class OutParam : public ObjectWrap {
public:
  static void Init(Handle<Object> target);
  static uni::CallbackType New(const uni::FunctionCallbackInfo& args);
  static Persistent<FunctionTemplate> constructorTemplate;
  int _type;
  int _size;
  inout_t _inOut;
  OutParam();
  ~OutParam();
  
  int type();  
  int size();  
  static const int OCCIINT = 0;
  static const int OCCISTRING = 1;
  static const int OCCIDOUBLE = 2;
  static const int OCCIFLOAT = 3;
  static const int OCCICURSOR = 4;
  static const int OCCICLOB = 5;
  static const int OCCIDATE = 6;
  static const int OCCITIMESTAMP = 7;
  static const int OCCINUMBER = 8;
  static const int OCCIBLOB = 9;
  static const int OCCIVECTOR = 10;

private:
};

#endif
