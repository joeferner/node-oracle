
#include "connection.h"
#include "executeBaton.h"
#include "commitBaton.h"
#include "rollbackBaton.h"
#include "outParam.h"
#include <vector>
#include <node_version.h>

Persistent<FunctionTemplate> Connection::constructorTemplate;

void Connection::Init(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  constructorTemplate = Persistent<FunctionTemplate>::New(t);
  constructorTemplate->InstanceTemplate()->SetInternalFieldCount(1);
  constructorTemplate->SetClassName(String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "execute", Execute);
  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "setAutoCommit", SetAutoCommit);
  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "commit", Commit);
  NODE_SET_PROTOTYPE_METHOD(constructorTemplate, "rollback", Rollback);

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

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Execute, EIO_AfterExecute);

  connection->Ref();

  return Undefined();
}

Handle<Value> Connection::Close(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
  connection->closeConnection();

  return Undefined();
}

Handle<Value> Connection::Commit(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_FUN_ARG(0, callback);

  CommitBaton* baton;
  try {
    baton = new CommitBaton(connection, &callback);
  } catch(NodeOracleException &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.getMessage().c_str()));
    argv[1] = Undefined();
    callback->Call(Context::GetCurrent()->Global(), 2, argv);
    return Undefined();
  }

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Commit, EIO_AfterCommit);

  connection->Ref();

  return Undefined();
}

Handle<Value> Connection::Rollback(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_FUN_ARG(0, callback);

  RollbackBaton* baton;
  try {
    baton = new RollbackBaton(connection, &callback);
  } catch(NodeOracleException &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.getMessage().c_str()));
    argv[1] = Undefined();
    callback->Call(Context::GetCurrent()->Global(), 2, argv);
    return Undefined();
  }

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Rollback, EIO_AfterRollback);

  connection->Ref();

  return Undefined();
}

Handle<Value> Connection::SetAutoCommit(const Arguments& args) {
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
  REQ_BOOL_ARG(0, autoCommit);
  connection->m_autoCommit = autoCommit;
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
      case oracle::occi::OCCI_TYPECODE_FLOAT:
      case oracle::occi::OCCI_TYPECODE_DOUBLE:
      case oracle::occi::OCCI_TYPECODE_REAL:
      case oracle::occi::OCCI_TYPECODE_DECIMAL:
      case oracle::occi::OCCI_TYPECODE_INTEGER:
      case oracle::occi::OCCI_TYPECODE_SMALLINT:
        col->type = VALUE_TYPE_NUMBER;
        break;
      case oracle::occi::OCCI_TYPECODE_VARCHAR2:
      case oracle::occi::OCCI_TYPECODE_VARCHAR:
      case oracle::occi::OCCI_TYPECODE_CHAR:
        col->type = VALUE_TYPE_STRING;
        break;
      case oracle::occi::OCCI_TYPECODE_CLOB:
        col->type = VALUE_TYPE_CLOB;
        break;
      case oracle::occi::OCCI_TYPECODE_DATE:
        col->type = VALUE_TYPE_DATE;
        break;
      case OCI_TYPECODE_TIMESTAMP:
        col->type = VALUE_TYPE_TIMESTAMP;
        break;
      case oracle::occi::OCCI_TYPECODE_BLOB:
        col->type = VALUE_TYPE_BLOB;
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
    if(rs->isNull(colIndex)) {
      row->values.push_back(NULL);
    } else {
      switch(col->type) {
        case VALUE_TYPE_STRING:
          row->values.push_back(new std::string(rs->getString(colIndex)));
          break;
        case VALUE_TYPE_NUMBER:
          row->values.push_back(new oracle::occi::Number(rs->getNumber(colIndex)));
          break;
        case VALUE_TYPE_DATE:
          row->values.push_back(new oracle::occi::Date(rs->getDate(colIndex)));
          break;
        case VALUE_TYPE_TIMESTAMP:
          row->values.push_back(new oracle::occi::Timestamp(rs->getTimestamp(colIndex)));
          break;
        case VALUE_TYPE_CLOB:
          row->values.push_back(new oracle::occi::Clob(rs->getClob(colIndex)));
          break;
        case VALUE_TYPE_BLOB:
          row->values.push_back(new oracle::occi::Blob(rs->getBlob(colIndex)));
          break;
        default:
          std::ostringstream message;
          message << "CreateRowFromCurrentResultSetRow: Unhandled type: " << col->type;
          throw NodeOracleException(message.str());
          break;
      }
    }
  }
  return row;
}

void Connection::EIO_Commit(uv_work_t* req) {
  CommitBaton* baton = static_cast<CommitBaton*>(req->data);

  baton->connection->m_connection->commit();
}

void Connection::EIO_AfterCommit(uv_work_t* req) {
  CommitBaton* baton = static_cast<CommitBaton*>(req->data);

  baton->connection->Unref();

  Handle<Value> argv[2];
  argv[0] = Undefined();
  baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  delete baton;
}

void Connection::EIO_Rollback(uv_work_t* req) {
  RollbackBaton* baton = static_cast<RollbackBaton*>(req->data);

  baton->connection->m_connection->rollback();
}

void Connection::EIO_AfterRollback(uv_work_t* req) {
  RollbackBaton* baton = static_cast<RollbackBaton*>(req->data);

  baton->connection->Unref();

  Handle<Value> argv[2];
  argv[0] = Undefined();
  baton->callback->Call(Context::GetCurrent()->Global(), 1, argv);

  delete baton;
}

void Connection::EIO_Execute(uv_work_t* req) {
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);

  baton->rows = NULL;
  baton->error = NULL;

  oracle::occi::Statement* stmt = NULL;
  oracle::occi::ResultSet* rs = NULL;
  try {
    //printf("%s\n", baton->sql.c_str());
    stmt = baton->connection->m_connection->createStatement(baton->sql);
    stmt->setAutoCommit(baton->connection->m_autoCommit);
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

void CallDateMethod(v8::Local<v8::Date> date, const char* methodName, int val) {
  Handle<Value> args[1];
  args[0] = Number::New(val);
  Local<Function>::Cast(date->Get(String::New(methodName)))->Call(date, 1, args);
}

Local<Date> OracleDateToV8Date(oracle::occi::Date* d) {
  int year;
  unsigned int month, day, hour, min, sec;
  d->getDate(year, month, day, hour, min, sec);
  Local<Date> date = Date::Cast(*Date::New(0.0));
  CallDateMethod(date, "setSeconds", sec);
  CallDateMethod(date, "setMinutes", min);
  CallDateMethod(date, "setHours", hour);
  CallDateMethod(date, "setDate", day);
  CallDateMethod(date, "setMonth", month - 1);
  CallDateMethod(date, "setFullYear", year);
  return date;
}

Local<Date> OracleTimestampToV8Date(oracle::occi::Timestamp* d) {
  int year;
  unsigned int month, day;
  d->getDate(year, month, day);
  Local<Date> date = Date::Cast(*Date::New(0.0));
  CallDateMethod(date, "setSeconds", 0);
  CallDateMethod(date, "setMinutes", 0);
  CallDateMethod(date, "setHours", 0);
  CallDateMethod(date, "setDate", day);
  CallDateMethod(date, "setMonth", month - 1);
  CallDateMethod(date, "setFullYear", year);
  return date;
}

Local<Object> Connection::CreateV8ObjectFromRow(ExecuteBaton* baton, row_t* currentRow) {
  Local<Object> obj = Object::New();
  uint32_t colIndex = 0;
  for (std::vector<column_t*>::iterator iterator = baton->columns.begin(), end = baton->columns.end(); iterator != end; ++iterator, colIndex++) {
    column_t* col = *iterator;
    void* val = currentRow->values[colIndex];
    if(val == NULL) {
      obj->Set(String::New(col->name.c_str()), Null());
    } else {
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
        case VALUE_TYPE_DATE:
          {
            oracle::occi::Date* v = (oracle::occi::Date*)val;
            obj->Set(String::New(col->name.c_str()), OracleDateToV8Date(v));
          }
          break;
        case VALUE_TYPE_TIMESTAMP:
          {
            oracle::occi::Timestamp* v = (oracle::occi::Timestamp*)val;
            obj->Set(String::New(col->name.c_str()), OracleTimestampToV8Date(v));
          }
          break;
        case VALUE_TYPE_CLOB:
          {
            oracle::occi::Clob* v = (oracle::occi::Clob*)val;
            v->open(oracle::occi::OCCI_LOB_READONLY);
            v->setCharSetForm(oracle::occi::OCCI_SQLCS_NCHAR);

            int clobLength = v->length();
            oracle::occi::Stream *instream = v->getStream(1,0);
            char *buffer = new char[clobLength];
            memset(buffer, NULL, clobLength);
            instream->readBuffer(buffer, clobLength);
            v->closeStream(instream);

            obj->Set(String::New(col->name.c_str()), String::New(buffer));
          }
          break;
        case VALUE_TYPE_BLOB:
          {
            //oracle::occi::Blob* v = (oracle::occi::Blob*)val;
            obj->Set(String::New(col->name.c_str()), Null()); // TODO: handle blobs
          }
          break;
        default:
          std::ostringstream message;
          message << "CreateV8ObjectFromRow: Unhandled type: " << col->type;
          throw NodeOracleException(message.str());
          break;
      }
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

void Connection::EIO_AfterExecute(uv_work_t* req) {
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);

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
}

void Connection::setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection) {
  m_environment = environment;
  m_connection = connection;
}
