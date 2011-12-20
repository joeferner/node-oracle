
#include "connection.h"
#include "outParam.h"
#include <vector>

Persistent<FunctionTemplate> Connection::constructorTemplate;

#define VALUE_TYPE_OUTPUT 1
#define VALUE_TYPE_STRING 2

struct column_t {
  int type;
  std::string name;
};

struct row_t {
  std::vector<void*> values;
};

struct value_t {
  int type;
  void* value;
};

struct execute_baton_t {
  Connection *connection;
  Persistent<Function> callback;
  std::vector<value_t*> values;
  std::string sql;
  std::vector<column_t*> columns;
  std::vector<row_t*>* rows;
  std::string* error;
  int updateCount;
  int* returnParam;
};

void Connection::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructorTemplate = Persistent<FunctionTemplate>::New(t);
  constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  constructorTemplate->SetClassName(String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "execute", Execute);
  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "close", Close);

  target->Set(String::NewSymbol("Connection"), constructorTemplate->GetFunction());
}

Handle<Value> Connection::New(const Arguments& args) {
  HandleScope scope;

  Connection *connection = new Connection();
  connection->Wrap(args.This());
  return args.This();
}

Connection::Connection() {
}

Connection::~Connection() {
  closeConnection();
}

Handle<Value> Connection::Execute(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_STRING_ARG(0, sql);
  REQ_ARRAY_ARG(1, values);
  REQ_FUN_ARG(2, callback);

  String::AsciiValue sqlVal(sql);

  execute_baton_t* baton = new execute_baton_t();
  baton->connection = connection;
  baton->sql = *sqlVal;
  baton->callback = Persistent<Function>::New(callback);
  baton->returnParam = NULL;

  for(uint32_t i=0; i<values->Length(); i++) {
    Local<Value> val = values->Get(i);

    value_t *value = new value_t();
    if(val->IsString()) {
      String::AsciiValue asciiVal(val);
      value->type = VALUE_TYPE_STRING;
      value->value = new std::string(*asciiVal);
      baton->values.push_back(value);
    } else if(val->IsObject() && val->ToObject()->FindInstanceInPrototypeChain(OutParam::constructorTemplate) != Null()) {
      value->type = VALUE_TYPE_OUTPUT;
      value->value = NULL;
      baton->values.push_back(value);
    } else {
      Handle<Value> argv[2];
      argv[0] = Exception::Error(String::New("unhandled value type"));
      argv[1] = Undefined();
      baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
      return Undefined();
    }
  }

  eio_custom(EIO_Execute, EIO_PRI_DEFAULT, EIO_AfterExecute, baton);
  ev_ref(EV_DEFAULT_UC);

  connection->Ref();

  return Undefined();
}

Handle<Value> Connection::Close(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
  connection->closeConnection();

  return Undefined();
}

void Connection::closeConnection() {
  if(m_environment && m_connection) {
    m_environment->terminateConnection(m_connection);
    m_connection = NULL;
  }
}

void Connection::EIO_Execute(eio_req* req) {
  execute_baton_t* baton = static_cast<execute_baton_t*>(req->data);

  baton->rows = NULL;
  baton->error = NULL;

  oracle::occi::Statement* stmt = NULL;
  oracle::occi::ResultSet* rs = NULL;
  try {
    //printf("%s\n", baton->sql.c_str());
    stmt = baton->connection->m_connection->createStatement(baton->sql);
    uint32_t index = 1;
    int outputParam = -1;
    for (std::vector<value_t*>::iterator iterator = baton->values.begin(), end = baton->values.end(); iterator != end; ++iterator, index++) {
      value_t* val = *iterator;
      switch(val->type) {
        case VALUE_TYPE_STRING:
          stmt->setString(index, *((std::string*)val->value));
          break;
        case VALUE_TYPE_OUTPUT:
          stmt->registerOutParam(index, oracle::occi::OCCIINT);
          outputParam = index;
          break;
        default:
          baton->error = new std::string("unhandled value type");
          goto error;
      }
    }

    int status = stmt->execute();
    if(status == oracle::occi::Statement::UPDATE_COUNT_AVAILABLE) {
      baton->updateCount = stmt->getUpdateCount();
      if(outputParam >= 0) {
        baton->returnParam = new int;
        *(baton->returnParam) = stmt->getInt(outputParam);
      }
    } else {
      rs = stmt->executeQuery();
      std::vector<oracle::occi::MetaData> columns = rs->getColumnListMetaData();
      for (std::vector<oracle::occi::MetaData>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator) {
        oracle::occi::MetaData metadata = *iterator;
        column_t* col = new column_t();
        col->name = metadata.getString(oracle::occi::MetaData::ATTR_NAME);
        int type = metadata.getInt(oracle::occi::MetaData::ATTR_DATA_TYPE);
        switch(type) {
          default:
            col->type = VALUE_TYPE_STRING;
            break;
        }
        baton->columns.push_back(col);
      }

      baton->rows = new std::vector<row_t*>();

      while(rs->next()) {
        row_t* row = new row_t();
        int colIndex = 1;
        for (std::vector<column_t*>::iterator iterator = baton->columns.begin(), end = baton->columns.end(); iterator != end; ++iterator, colIndex++) {
          column_t* col = *iterator;
          switch(col->type) {
            default:
            case VALUE_TYPE_STRING:
              row->values.push_back(new std::string(rs->getString(colIndex)));
              break;
          }
        }
        baton->rows->push_back(row);
      }
    }
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new std::string(ex.getMessage());
  }

error:
  if(stmt && rs) {
    stmt->closeResultSet(rs);
  }
  if(stmt) {
    baton->connection->m_connection->terminateStatement(stmt);
  }
}

int Connection::EIO_AfterExecute(eio_req* req) {
  execute_baton_t* baton = static_cast<execute_baton_t*>(req->data);
  ev_unref(EV_DEFAULT_UC);
  baton->connection->Unref();

  Handle<Value> argv[2];
  if(baton->error) {
    argv[0] = Exception::Error(String::New(baton->error->c_str()));
    argv[1] = Undefined();
  } else {
    argv[0] = Undefined();

    if(baton->rows) {
      size_t totalRows = baton->rows->size();
      Local<Array> rows = Array::New(totalRows);
      uint32_t index = 0;
      for (std::vector<row_t*>::iterator iterator = baton->rows->begin(), end = baton->rows->end(); iterator != end; ++iterator, index++) {
        row_t* currentRow = *iterator;
        Local<Object> obj = Object::New();
        uint32_t colIndex = 0;
        for (std::vector<column_t*>::iterator iterator = baton->columns.begin(), end = baton->columns.end(); iterator != end; ++iterator, colIndex++) {
          column_t* col = *iterator;
          void* val = currentRow->values[colIndex];
          switch(col->type) {
            case VALUE_TYPE_STRING:
              std::string* v = (std::string*)val;
              obj->Set(String::New(col->name.c_str()), String::New(v->c_str()));
              delete v;
              break;
          }
        }
        rows->Set(index, obj);
        delete currentRow;
      }
      delete baton->rows;
      argv[1] = rows;
    } else {
      Local<Object> obj = Object::New();
      obj->Set(String::New("updateCount"), Integer::New(baton->updateCount));
      if(baton->returnParam) {
        obj->Set(String::New("returnParam"), Integer::New(*baton->returnParam));
        delete baton->returnParam;
      }
      argv[1] = obj;
    }
  }
  baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  baton->callback.Dispose();

  for (std::vector<column_t*>::iterator iterator = baton->columns.begin(), end = baton->columns.end(); iterator != end; ++iterator) {
    column_t* col = *iterator;
    delete col;
  }

  for (std::vector<value_t*>::iterator iterator = baton->values.begin(), end = baton->values.end(); iterator != end; ++iterator) {
    value_t* val = *iterator;
    if(val->type == VALUE_TYPE_STRING) {
      delete (std::string*)val->value;
    }
    delete val;
  }

  if(baton->error) delete baton->error;

  delete baton;
  return 0;
}

void Connection::setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection) {
  m_environment = environment;
  m_connection = connection;
}
