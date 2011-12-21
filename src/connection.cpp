
#include "connection.h"
#include "outParam.h"
#include <vector>

Persistent<FunctionTemplate> Connection::constructorTemplate;

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

  ExecuteBaton* baton;
  try {
    baton = new ExecuteBaton(connection, *sqlVal, &values, &callback);
  } catch(NodeOracleException &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.getMessage().c_str()));
    argv[1] = Undefined();
    callback->Call(Context::GetCurrent()->Global(), 2, argv);
    return Undefined();
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

int Connection::SetValuesOnStatement(oracle::occi::Statement* stmt, std::vector<value_t*> &values) {
  uint32_t index = 1;
  int outputParam = -1;
  for (std::vector<value_t*>::iterator iterator = values.begin(), end = values.end(); iterator != end; ++iterator, index++) {
    value_t* val = *iterator;
    switch(val->type) {
      case VALUE_TYPE_NULL:
        stmt->setNull(index, oracle::occi::OCCISTRING);
        break;
      case VALUE_TYPE_STRING:
        stmt->setString(index, *((std::string*)val->value));
        break;
      case VALUE_TYPE_NUMBER:
        stmt->setNumber(index, *((oracle::occi::Number*)val->value));
        break;
      case VALUE_TYPE_DATE:
        stmt->setDate(index, *((oracle::occi::Date*)val->value));
        break;
      case VALUE_TYPE_OUTPUT:
        stmt->registerOutParam(index, oracle::occi::OCCIINT);
        outputParam = index;
        break;
      default:
        throw NodeOracleException("SetValuesOnStatement: Unhandled value type");
    }
  }
  return outputParam;
}

void Connection::CreateColumnsFromResultSet(oracle::occi::ResultSet* rs, std::vector<column_t*> &columns) {
  std::vector<oracle::occi::MetaData> metadata = rs->getColumnListMetaData();
  for (std::vector<oracle::occi::MetaData>::iterator iterator = metadata.begin(), end = metadata.end(); iterator != end; ++iterator) {
    oracle::occi::MetaData metadata = *iterator;
    column_t* col = new column_t();
    col->name = metadata.getString(oracle::occi::MetaData::ATTR_NAME);
    int type = metadata.getInt(oracle::occi::MetaData::ATTR_DATA_TYPE);
    switch(type) {
      case oracle::occi::OCCI_TYPECODE_NUMBER:
        col->type = VALUE_TYPE_NUMBER;
        break;
      case oracle::occi::OCCI_TYPECODE_VARCHAR2:
      case oracle::occi::OCCI_TYPECODE_VARCHAR:
        col->type = VALUE_TYPE_STRING;
        break;
      default:
        std::ostringstream message;
        message << "CreateColumnsFromResultSet: Unhandled oracle data type: " << type;
        throw NodeOracleException(message.str());
        break;
    }
    columns.push_back(col);
  }
}

row_t* Connection::CreateRowFromCurrentResultSetRow(oracle::occi::ResultSet* rs, std::vector<column_t*> &columns) {
  row_t* row = new row_t();
  int colIndex = 1;
  for (std::vector<column_t*>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator, colIndex++) {
    column_t* col = *iterator;
    switch(col->type) {
      case VALUE_TYPE_STRING:
        row->values.push_back(new std::string(rs->getString(colIndex)));
        break;
      case VALUE_TYPE_NUMBER:
        row->values.push_back(new oracle::occi::Number(rs->getNumber(colIndex)));
        break;
      default:
        std::ostringstream message;
        message << "Unhandled type: " << col->type;
        throw NodeOracleException(message.str());
        break;
    }
  }
  return row;
}

void Connection::EIO_Execute(eio_req* req) {
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);

  baton->rows = NULL;
  baton->error = NULL;

  oracle::occi::Statement* stmt = NULL;
  oracle::occi::ResultSet* rs = NULL;
  try {
    //printf("%s\n", baton->sql.c_str());
    stmt = baton->connection->m_connection->createStatement(baton->sql);
    int outputParam = SetValuesOnStatement(stmt, baton->values);

    int status = stmt->execute();
    if(status == oracle::occi::Statement::UPDATE_COUNT_AVAILABLE) {
      baton->updateCount = stmt->getUpdateCount();
      if(outputParam >= 0) {
        baton->returnParam = new int;
        *(baton->returnParam) = stmt->getInt(outputParam);
      }
    } else {
      rs = stmt->executeQuery();
      CreateColumnsFromResultSet(rs, baton->columns);
      baton->rows = new std::vector<row_t*>();

      while(rs->next()) {
        row_t* row = CreateRowFromCurrentResultSetRow(rs, baton->columns);
        baton->rows->push_back(row);
      }
    }
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new std::string(ex.getMessage());
  } catch(NodeOracleException &ex) {
    baton->error = new std::string(ex.getMessage());
  }

  if(stmt && rs) {
    stmt->closeResultSet(rs);
  }
  if(stmt) {
    baton->connection->m_connection->terminateStatement(stmt);
  }
}

Local<Object> Connection::CreateV8ObjectFromRow(ExecuteBaton* baton, row_t* currentRow) {
  Local<Object> obj = Object::New();
  uint32_t colIndex = 0;
  for (std::vector<column_t*>::iterator iterator = baton->columns.begin(), end = baton->columns.end(); iterator != end; ++iterator, colIndex++) {
    column_t* col = *iterator;
    void* val = currentRow->values[colIndex];
    switch(col->type) {
      case VALUE_TYPE_STRING:
        {
          std::string* v = (std::string*)val;
          obj->Set(String::New(col->name.c_str()), String::New(v->c_str()));
          delete v;
        }
        break;
      case VALUE_TYPE_NUMBER:
        {
          oracle::occi::Number* v = (oracle::occi::Number*)val;
          obj->Set(String::New(col->name.c_str()), Number::New((double)(*v)));
          delete v;
        }
        break;
      default:
        std::ostringstream message;
        message << "Unhandled type: " << col->type;
        throw NodeOracleException(message.str());
        break;
    }
  }
  return obj;
}

Local<Array> Connection::CreateV8ArrayFromRows(ExecuteBaton* baton) {
  size_t totalRows = baton->rows->size();
  Local<Array> rows = Array::New(totalRows);
  uint32_t index = 0;
  for (std::vector<row_t*>::iterator iterator = baton->rows->begin(), end = baton->rows->end(); iterator != end; ++iterator, index++) {
    row_t* currentRow = *iterator;
    Local<Object> obj = CreateV8ObjectFromRow(baton, currentRow);
    rows->Set(index, obj);
  }
  return rows;
}

int Connection::EIO_AfterExecute(eio_req* req) {
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);
  ev_unref(EV_DEFAULT_UC);
  baton->connection->Unref();

  try {
    Handle<Value> argv[2];
    if(baton->error) {
      argv[0] = Exception::Error(String::New(baton->error->c_str()));
      argv[1] = Undefined();
    } else {
      argv[0] = Undefined();
      if(baton->rows) {
        argv[1] = CreateV8ArrayFromRows(baton);
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
  } catch(NodeOracleException &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.getMessage().c_str()));
    argv[1] = Undefined();
    baton->callback->Call(Context::GetCurrent()->Global(), 2, argv);
  }

  delete baton;
  return 0;
}

void Connection::setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection) {
  m_environment = environment;
  m_connection = connection;
}
