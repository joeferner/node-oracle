#include <string.h>
#include "connection.h"
#include "executeBaton.h"
#include "commitBaton.h"
#include "rollbackBaton.h"
#include "statement.h"
#include "reader.h"
#include "outParam.h"
#include <vector>
#include <node_version.h>
#include <iostream>
using namespace std;

Persistent<FunctionTemplate> Connection::constructorTemplate;

void Connection::Init(Handle<Object> target) {
  UNI_SCOPE(scope);

  Local<FunctionTemplate> t = FunctionTemplate::New(New);
  uni::Reset(constructorTemplate, t);
  uni::Deref(constructorTemplate)->InstanceTemplate()->SetInternalFieldCount(1);
  uni::Deref(constructorTemplate)->SetClassName(String::NewSymbol("Connection"));

  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "execute", Execute);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "executeSync", ExecuteSync);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "readerHandle", CreateReader);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "prepare", Prepare);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "close", Close);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "isConnected", IsConnected);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "setAutoCommit", SetAutoCommit);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "setPrefetchRowCount", SetPrefetchRowCount);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "commit", Commit);
  NODE_SET_PROTOTYPE_METHOD(uni::Deref(constructorTemplate), "rollback", Rollback);

  target->Set(String::NewSymbol("Connection"), uni::Deref(constructorTemplate)->GetFunction());
}

uni::CallbackType Connection::New(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);

  Connection *connection = new Connection();
  connection->Wrap(args.This());
  UNI_RETURN(scope, args, args.This());
}

Connection::Connection():m_connection(NULL), m_environment(NULL), m_autoCommit(true), m_prefetchRowCount(0) {
}

Connection::~Connection() {
  closeConnection();
}

uni::CallbackType Connection::Execute(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_STRING_ARG(0, sql);
  REQ_ARRAY_ARG(1, values);
  REQ_FUN_ARG(2, callback);

  String::Utf8Value sqlVal(sql);

  ExecuteBaton* baton = new ExecuteBaton(connection, *sqlVal, &values, &callback);
  if (baton->error) {
    Local<String> message = String::New(baton->error->c_str());
    delete baton;
    UNI_THROW(Exception::Error(message));
  }

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Execute, (uv_after_work_cb)EIO_AfterExecute);

  connection->Ref();

  UNI_RETURN(scope, args, Undefined());
}

uni::CallbackType Connection::Prepare(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_STRING_ARG(0, sql);

  String::Utf8Value sqlVal(sql);

  StatementBaton* baton = new StatementBaton(connection, *sqlVal, NULL);

  Local<Object> statementHandle(uni::Deref(Statement::constructorTemplate)->GetFunction()->NewInstance());
  Statement* statement = ObjectWrap::Unwrap<Statement>(statementHandle);
  statement->setBaton(baton);

  UNI_RETURN(scope, args, statementHandle);
}

uni::CallbackType Connection::CreateReader(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_STRING_ARG(0, sql);
  REQ_ARRAY_ARG(1, values);

  String::Utf8Value sqlVal(sql);

  ReaderBaton* baton = new ReaderBaton(connection, *sqlVal, &values);

  Local<Object> readerHandle(uni::Deref(Reader::constructorTemplate)->GetFunction()->NewInstance());
  Reader* reader = ObjectWrap::Unwrap<Reader>(readerHandle);
  reader->setBaton(baton);

  UNI_RETURN(scope, args, readerHandle);
}

uni::CallbackType Connection::Close(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  try {
    Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
    connection->closeConnection();

    UNI_RETURN(scope, args, Undefined());
  } catch (const exception& ex) {
    UNI_THROW(Exception::Error(String::New(ex.what())));
  }
}

uni::CallbackType Connection::IsConnected(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  if(connection && connection->m_connection) {
    UNI_RETURN(scope, args, Boolean::New(true));
  } else {
    UNI_RETURN(scope, args, Boolean::New(false));
  }
}

uni::CallbackType Connection::Commit(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_FUN_ARG(0, callback);

  CommitBaton* baton = new CommitBaton(connection, &callback);

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Commit, (uv_after_work_cb)EIO_AfterCommit);

  connection->Ref();

  UNI_RETURN(scope, args, Undefined());
}

uni::CallbackType Connection::Rollback(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_FUN_ARG(0, callback);

  RollbackBaton* baton = new RollbackBaton(connection, &callback);

  uv_work_t* req = new uv_work_t();
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, EIO_Rollback, (uv_after_work_cb)EIO_AfterRollback);

  connection->Ref();

  UNI_RETURN(scope, args, Undefined());
}

uni::CallbackType Connection::SetAutoCommit(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
  REQ_BOOL_ARG(0, autoCommit);
  connection->m_autoCommit = autoCommit;
  UNI_RETURN(scope, args, Undefined());
}

uni::CallbackType Connection::SetPrefetchRowCount(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());
  REQ_INT_ARG(0, prefetchRowCount);
  connection->m_prefetchRowCount = prefetchRowCount;
  UNI_RETURN(scope, args, Undefined());
}

void Connection::closeConnection() {
  if(m_environment && m_connection) {
    try {
      m_environment->terminateConnection(m_connection);
      m_connection = NULL;
    } catch (oracle::occi::SQLException &ex) {
      m_connection = NULL;
      throw; // rethrow to allow caller to catch and rethrow in V8 scope
    }
  }
}

void RandomBytesFree(char* data, void* hint) {
  delete[] data;
}

int Connection::SetValuesOnStatement(oracle::occi::Statement* stmt, ExecuteBaton* baton) {
  std::vector<value_t*> &values = baton->values;
  uint32_t index = 1;
  int outputParam = -1;
  OutParam * outParam = NULL;
  for (vector<value_t*>::iterator iterator = values.begin(), end = values.end(); iterator != end; ++iterator, index++) {
    value_t* val = *iterator;
    int outParamType;

    switch(val->type) {
      case VALUE_TYPE_NULL:
        stmt->setNull(index, oracle::occi::OCCISTRING);
        break;
      case VALUE_TYPE_STRING:
        stmt->setString(index, *((string*)val->value));
        break;
      case VALUE_TYPE_NUMBER:
        stmt->setNumber(index, *((oracle::occi::Number*)val->value));
        break;
      case VALUE_TYPE_TIMESTAMP:
        stmt->setTimestamp(index, *((oracle::occi::Timestamp*)val->value));
        break;
      case VALUE_TYPE_OUTPUT:
        outParam = static_cast<OutParam*>(val->value);
        // std::cout << "OutParam B: " << outParam << " "<< outParam->type() << " " << outParam->_inOut.hasInParam << std::endl;
        outParamType = outParam->type();
        switch(outParamType) {
          case OutParam::OCCIINT:
            if (outParam->_inOut.hasInParam) {
              stmt->setInt(index, outParam->_inOut.intVal);
            }else {
              stmt->registerOutParam(index, oracle::occi::OCCIINT);
            }
            break;
          case OutParam::OCCISTRING:
            if (outParam->_inOut.hasInParam) {
              stmt->setString(index, outParam->_inOut.stringVal);
            }else {
              stmt->registerOutParam(index, oracle::occi::OCCISTRING, outParam->size());
            }
            break;
          case OutParam::OCCIDOUBLE:
            if (outParam->_inOut.hasInParam) {
              stmt->setDouble(index, outParam->_inOut.doubleVal);
            }else {
              stmt->registerOutParam(index, oracle::occi::OCCIDOUBLE);
            }
            break;
          case OutParam::OCCIFLOAT:
            if (outParam->_inOut.hasInParam) {
              stmt->setFloat(index, outParam->_inOut.floatVal);
            }else {
              stmt->registerOutParam(index, oracle::occi::OCCIFLOAT);
            }
            break;
          case OutParam::OCCICURSOR:
            stmt->registerOutParam(index, oracle::occi::OCCICURSOR);
            break;
          case OutParam::OCCICLOB:
            stmt->registerOutParam(index, oracle::occi::OCCICLOB);
            break;
          case OutParam::OCCIDATE:
            stmt->registerOutParam(index, oracle::occi::OCCIDATE);
            break;
          case OutParam::OCCITIMESTAMP:
            stmt->registerOutParam(index, oracle::occi::OCCITIMESTAMP);
            break;
          case OutParam::OCCINUMBER:
            {
              if (outParam->_inOut.hasInParam) {
                stmt->setNumber(index, outParam->_inOut.numberVal);
              } else {
                stmt->registerOutParam(index, oracle::occi::OCCINUMBER);
              }
              break;
            }
          case OutParam::OCCIBLOB:
            stmt->registerOutParam(index, oracle::occi::OCCIBLOB);
            break;
          default:
            {
              ostringstream oss;
              oss << "SetValuesOnStatement: Unknown OutParam type: " << outParamType;
              baton->error = new std::string(oss.str());
              return -2;
            }
        }
        outputParam = index;
        break;
      default:
        baton->error = new std::string("SetValuesOnStatement: Unhandled value type");
        return -2;
    }
  }
  return outputParam;
}

void Connection::CreateColumnsFromResultSet(oracle::occi::ResultSet* rs, ExecuteBaton* baton, vector<column_t*> &columns) {
  vector<oracle::occi::MetaData> metadata = rs->getColumnListMetaData();
  for (vector<oracle::occi::MetaData>::iterator iterator = metadata.begin(), end = metadata.end(); iterator != end; ++iterator) {
    oracle::occi::MetaData metadata = *iterator;
    column_t* col = new column_t();
    col->name = metadata.getString(oracle::occi::MetaData::ATTR_NAME);
    int type = metadata.getInt(oracle::occi::MetaData::ATTR_DATA_TYPE);
    col->charForm = metadata.getInt(oracle::occi::MetaData::ATTR_CHARSET_FORM);
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
        //Use OCI_TYPECODE from oro.h because occiCommon.h does not re-export these in its TypeCode enum
      case OCI_TYPECODE_TIMESTAMP:
      case OCI_TYPECODE_TIMESTAMP_TZ: //Timezone
      case OCI_TYPECODE_TIMESTAMP_LTZ: //Local Timezone
        col->type = VALUE_TYPE_TIMESTAMP;
        break;
      case oracle::occi::OCCI_TYPECODE_BLOB:
        col->type = VALUE_TYPE_BLOB;
        break;
      // The lines below are temporary mappings for RAW and ROWID types
      // I need them to test because my test db has columns like this but this mapping will need to be reviewed
      case 23: // see http://docs.oracle.com/cd/B14117_01/appdev.101/b10779/oci03typ.htm#421756
        //printf("datatype 23 for column %s\n", col->name.c_str());
        col->type = VALUE_TYPE_STRING;
        break;
      case 104: // rowid
        col->type = VALUE_TYPE_STRING;
        break;
      default:
        ostringstream message;
        message << "CreateColumnsFromResultSet: Unhandled oracle data type: " << type;
        delete col;
        baton->error = new std::string(message.str());
        return;
    }
    columns.push_back(col);
  }
}

row_t* Connection::CreateRowFromCurrentResultSetRow(oracle::occi::ResultSet* rs, ExecuteBaton* baton, vector<column_t*> &columns) {
  row_t* row = new row_t();
  int colIndex = 1;
  for (vector<column_t*>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator, colIndex++) {
    column_t* col = *iterator;
    if(rs->isNull(colIndex)) {
      row->values.push_back(NULL);
    } else {
      switch(col->type) {
        case VALUE_TYPE_STRING:
          row->values.push_back(new string(rs->getString(colIndex)));
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
          ostringstream message;
          message << "CreateRowFromCurrentResultSetRow: Unhandled type: " << col->type;
          baton->error = new std::string(message.str());
          delete row;
          return NULL;
      }
    }
  }
  return row;
}

void Connection::EIO_Commit(uv_work_t* req) {
  CommitBaton* baton = static_cast<CommitBaton*>(req->data);

  baton->connection->m_connection->commit();
}

void Connection::EIO_AfterCommit(uv_work_t* req, int status) {
  UNI_SCOPE(scope);
  CommitBaton* baton = static_cast<CommitBaton*>(req->data);

  baton->connection->Unref();

  Handle<Value> argv[2];
  argv[0] = Undefined();
  argv[1] = Undefined();
  node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->callback), 2, argv);
  delete baton;
  delete req;
}

void Connection::EIO_Rollback(uv_work_t* req) {
  RollbackBaton* baton = static_cast<RollbackBaton*>(req->data);

  baton->connection->m_connection->rollback();
}

void Connection::EIO_AfterRollback(uv_work_t* req, int status) {
  UNI_SCOPE(scope);
  RollbackBaton* baton = static_cast<RollbackBaton*>(req->data);

  baton->connection->Unref();

  Handle<Value> argv[2];
  argv[0] = Undefined();
  argv[1] = Undefined();
  node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->callback), 2, argv);
  delete baton;
  delete req;
}

oracle::occi::Statement* Connection::CreateStatement(ExecuteBaton* baton) {
  baton->rows = NULL;
  baton->error = NULL;

  if (! baton->connection->m_connection) {
    baton->error = new std::string("Connection already closed");
    return NULL;
  }
  try {
    oracle::occi::Statement* stmt = baton->connection->m_connection->createStatement(baton->sql);
    stmt->setAutoCommit(baton->connection->m_autoCommit);
    if (baton->connection->m_prefetchRowCount > 0) stmt->setPrefetchRowCount(baton->connection->m_prefetchRowCount);
    return stmt;
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new string(ex.getMessage());
    return NULL;
  }
}

void Connection::ExecuteStatement(ExecuteBaton* baton, oracle::occi::Statement* stmt) {
  oracle::occi::ResultSet* rs = NULL;

  int outputParam = SetValuesOnStatement(stmt, baton);
  if (baton->error) goto cleanup;

  if (!baton->outputs) baton->outputs = new std::vector<output_t*>();

  try {
    int status = stmt->execute();
    if(status == oracle::occi::Statement::UPDATE_COUNT_AVAILABLE) {
      baton->updateCount = stmt->getUpdateCount();
      if(outputParam >= 0) {
        for (vector<output_t*>::iterator iterator = baton->outputs->begin(), end = baton->outputs->end(); iterator != end; ++iterator) {
          output_t* output = *iterator;
          oracle::occi::ResultSet* rs;
          switch(output->type) {
            case OutParam::OCCIINT:
              output->intVal = stmt->getInt(output->index);
              break;
            case OutParam::OCCISTRING:
              output->strVal = string(stmt->getString(output->index));
              break;
            case OutParam::OCCIDOUBLE:
              output->doubleVal = stmt->getDouble(output->index);
              break;
            case OutParam::OCCIFLOAT:
              output->floatVal = stmt->getFloat(output->index);
              break;
            case OutParam::OCCICURSOR:
              rs = stmt->getCursor(output->index);
              CreateColumnsFromResultSet(rs, baton, output->columns);
              if (baton->error) goto cleanup;
              output->rows = new vector<row_t*>();
              while(rs->next()) {
                row_t* row = CreateRowFromCurrentResultSetRow(rs, baton, output->columns);
                if (baton->error) goto cleanup;
                output->rows->push_back(row);
              }
              break;
            case OutParam::OCCICLOB:
              output->clobVal = stmt->getClob(output->index);
              break;
            case OutParam::OCCIBLOB:
              output->blobVal = stmt->getBlob(output->index);
              break;
            case OutParam::OCCIDATE:
              output->dateVal = stmt->getDate(output->index);
              break;
            case OutParam::OCCITIMESTAMP:
              output->timestampVal = stmt->getTimestamp(output->index);
              break;
            case OutParam::OCCINUMBER:
              output->numberVal = stmt->getNumber(output->index);
              break;
            default:
              {
                ostringstream oss;
                oss << "Unknown OutParam type: " << output->type;
                baton->error = new std::string(oss.str());
                goto cleanup;
              }
          }
        }
      }
    } else if(status == oracle::occi::Statement::RESULT_SET_AVAILABLE) {
      rs = stmt->getResultSet();
      CreateColumnsFromResultSet(rs, baton, baton->columns);
      if (baton->error) goto cleanup;
      baton->rows = new vector<row_t*>();

      while(rs->next()) {
        row_t* row = CreateRowFromCurrentResultSetRow(rs, baton, baton->columns);
        if (baton->error) goto cleanup;
        baton->rows->push_back(row);
      }
    }
  } catch(oracle::occi::SQLException &ex) {
    baton->error = new string(ex.getMessage());
  } catch (const exception& ex) {
    baton->error = new string(ex.what());
  } catch (...) {
    baton->error = new string("Unknown Error");
  }
cleanup:
  if (rs) {
    stmt->closeResultSet(rs);
    rs = NULL;
  }
}

void Connection::EIO_Execute(uv_work_t* req) {
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);

  oracle::occi::Statement* stmt = CreateStatement(baton);
  if (baton->error) return;

  ExecuteStatement(baton, stmt);

  if (stmt) {
    if (baton->connection->m_connection) {
      baton->connection->m_connection->terminateStatement(stmt);
    }
    stmt = NULL;
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
  Local<Date> date = uni::DateCast(Date::New(0.0));
  CallDateMethod(date, "setUTCMilliseconds", 0);
  CallDateMethod(date, "setUTCSeconds", sec);
  CallDateMethod(date, "setUTCMinutes", min);
  CallDateMethod(date, "setUTCHours", hour);
  CallDateMethod(date, "setUTCDate", day);
  CallDateMethod(date, "setUTCMonth", month - 1);
  CallDateMethod(date, "setUTCFullYear", year);
  return date;
}

Local<Date> OracleTimestampToV8Date(oracle::occi::Timestamp* d) {
  int year;
  unsigned int month, day, hour, min, sec, fs, ms;
  d->getDate(year, month, day);
  d->getTime(hour, min, sec, fs);
  //occi always returns nanoseconds, regardless of precision set on timestamp column
  ms = (fs / 1000000.0) + 0.5; // add 0.5 to round to nearest millisecond

  Local<Date> date = uni::DateCast(Date::New(0.0));
  CallDateMethod(date, "setUTCMilliseconds", ms);
  CallDateMethod(date, "setUTCSeconds", sec);
  CallDateMethod(date, "setUTCMinutes", min);
  CallDateMethod(date, "setUTCHours", hour);
  CallDateMethod(date, "setUTCDate", day);
  CallDateMethod(date, "setUTCMonth", month - 1);
  CallDateMethod(date, "setUTCFullYear", year);
  return date;
}

Local<Object> Connection::CreateV8ObjectFromRow(ExecuteBaton* baton, vector<column_t*> columns, row_t* currentRow) {
  Local<Object> obj = Object::New();
  uint32_t colIndex = 0;
  for (vector<column_t*>::iterator iterator = columns.begin(), end = columns.end(); iterator != end; ++iterator, colIndex++) {
    column_t* col = *iterator;
    void* val = currentRow->values[colIndex];
    if(val == NULL) {
      obj->Set(String::New(col->name.c_str()), Null());
    } else {
      switch(col->type) {
        case VALUE_TYPE_STRING:
          {
            string* v = (string*)val;
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
            delete v;
          }
          break;
        case VALUE_TYPE_TIMESTAMP:
          {
            oracle::occi::Timestamp* v = (oracle::occi::Timestamp*)val;
            obj->Set(String::New(col->name.c_str()), OracleTimestampToV8Date(v));
            delete v;
          }
          break;
        case VALUE_TYPE_CLOB:
          {
            oracle::occi::Clob* v = (oracle::occi::Clob*)val;
            v->open(oracle::occi::OCCI_LOB_READONLY);

            switch(col->charForm) {
              case SQLCS_IMPLICIT:
                v->setCharSetForm(oracle::occi::OCCI_SQLCS_IMPLICIT);
                break;
              case SQLCS_NCHAR:
                v->setCharSetForm(oracle::occi::OCCI_SQLCS_NCHAR);
                break;
              case SQLCS_EXPLICIT:
                v->setCharSetForm(oracle::occi::OCCI_SQLCS_EXPLICIT);
                break;
              case SQLCS_FLEXIBLE:
                v->setCharSetForm(oracle::occi::OCCI_SQLCS_FLEXIBLE);
                break;
            }

            oracle::occi::Stream *instream = v->getStream(1,0);
            // chunk size is set when the table is created
            size_t chunkSize = v->getChunkSize();
            char *buffer = new char[chunkSize];
            memset(buffer, 0, chunkSize);
            std::string columnVal;
            int numBytesRead = instream->readBuffer(buffer, chunkSize);
            int totalBytesRead = 0;
            while (numBytesRead != -1) {
              totalBytesRead += numBytesRead;
              columnVal.append(buffer);
              numBytesRead = instream->readBuffer(buffer, chunkSize);
            }

            v->closeStream(instream);
            v->close();
            obj->Set(String::New(col->name.c_str()), String::New(columnVal.c_str(), totalBytesRead));
            delete v;
            delete [] buffer;
          }
          break;
        case VALUE_TYPE_BLOB:
          {
            oracle::occi::Blob* v = (oracle::occi::Blob*)val;
            v->open(oracle::occi::OCCI_LOB_READONLY);
            int blobLength = v->length();
            oracle::occi::Stream *instream = v->getStream(1,0);
            char *buffer = new char[blobLength];
            memset(buffer, 0, blobLength);
            instream->readBuffer(buffer, blobLength);
            v->closeStream(instream);
            v->close();

            // convert to V8 buffer
            uni::BufferType nodeBuff = node::Buffer::New(buffer, blobLength, RandomBytesFree, NULL);
            v8::Local<v8::Object> globalObj = v8::Context::GetCurrent()->Global();
            v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(v8::String::New("Buffer")));
            v8::Handle<v8::Value> constructorArgs[3] = { uni::BufferToHandle(nodeBuff), v8::Integer::New(blobLength), v8::Integer::New(0) };
            v8::Local<v8::Object> v8Buffer = bufferConstructor->NewInstance(3, constructorArgs);
            obj->Set(String::New(col->name.c_str()), v8Buffer);
            delete v;
            delete[] buffer;
            break;
          }
          break;
        default:
          ostringstream oss;
          oss << "CreateV8ObjectFromRow: Unhandled type: " << col->type;
          baton->error = new std::string(oss.str());
          return obj;
      }
    }
  }
  return obj;
}

Local<Array> Connection::CreateV8ArrayFromRows(ExecuteBaton* baton, vector<column_t*> columns, vector<row_t*>* rows) {
  size_t totalRows = rows->size();
  Local<Array> retRows = Array::New(totalRows);
  uint32_t index = 0;
  for (vector<row_t*>::iterator iterator = rows->begin(), end = rows->end(); iterator != end; ++iterator, index++) {
    row_t* currentRow = *iterator;
    Local<Object> obj = CreateV8ObjectFromRow(baton, columns, currentRow);
    if (baton->error) return retRows;
    retRows->Set(index, obj);
  }
  return retRows;
}

void Connection::EIO_AfterExecute(uv_work_t* req, int status) {

  UNI_SCOPE(scope);
  ExecuteBaton* baton = static_cast<ExecuteBaton*>(req->data);

  baton->connection->Unref();
  try {
    Handle<Value> argv[2];
    handleResult(baton, argv);
    node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->callback), 2, argv);
  } catch(const exception &ex) {
    Handle<Value> argv[2];
    argv[0] = Exception::Error(String::New(ex.what()));
    argv[1] = Undefined();
    node::MakeCallback(Context::GetCurrent()->Global(), uni::Deref(baton->callback), 2, argv);
  }

  delete baton;
  delete req;
}

void Connection::handleResult(ExecuteBaton* baton, Handle<Value> (&argv)[2]) {
  if(baton->error) {
failed:
    argv[0] = Exception::Error(String::New(baton->error->c_str()));
    argv[1] = Undefined();
  } else {
    argv[0] = Undefined();
    if(baton->rows) {
      argv[1] = CreateV8ArrayFromRows(baton, baton->columns, baton->rows);
      if (baton->error) goto failed; // delete argv[1] ??
    } else {
      Local<Object> obj = Object::New();
      obj->Set(String::New("updateCount"), Integer::New(baton->updateCount));

      /* Note: attempt to keep backward compatability here: existing users of this library will have code that expects a single out param
         called 'returnParam'. For multiple out params, the first output will continue to be called 'returnParam' and subsequent outputs
         will be called 'returnParamX'.
         */
      uint32_t index = 0;
      for (vector<output_t*>::iterator iterator = baton->outputs->begin(), end = baton->outputs->end(); iterator != end; ++iterator, index++) {
        output_t* output = *iterator;
        stringstream ss;
        ss << "returnParam";
        if(index > 0) ss << index;
        string returnParam(ss.str());
        switch(output->type) {
          case OutParam::OCCIINT:
            obj->Set(String::New(returnParam.c_str()), Integer::New(output->intVal));
            break;
          case OutParam::OCCISTRING:
            obj->Set(String::New(returnParam.c_str()), String::New(output->strVal.c_str()));
            break;
          case OutParam::OCCIDOUBLE:
            obj->Set(String::New(returnParam.c_str()), Number::New(output->doubleVal));
            break;
          case OutParam::OCCIFLOAT:
            obj->Set(String::New(returnParam.c_str()), Number::New(output->floatVal));
            break;
          case OutParam::OCCICURSOR:
            obj->Set(String::New(returnParam.c_str()), CreateV8ArrayFromRows(baton, output->columns, output->rows));
            if (baton->error) goto failed;
            break;
          case OutParam::OCCICLOB:
            {
              output->clobVal.open(oracle::occi::OCCI_LOB_READONLY);
      				oracle::occi::Stream* instream = output->clobVal.getStream(1,0);
      				size_t chunkSize = output->clobVal.getChunkSize();
      				char *buffer = new char[chunkSize];
      				memset(buffer, 0, chunkSize);
      				std::string clobVal;
      				int numBytesRead = instream->readBuffer(buffer, chunkSize);
      				int totalBytesRead = 0;
      				while (numBytesRead != -1) {
      					totalBytesRead += numBytesRead;
      					clobVal.append(buffer);
      					numBytesRead = instream->readBuffer(buffer, chunkSize);
      				}
      				output->clobVal.closeStream(instream);
      				output->clobVal.close();
      				obj->Set(String::New(returnParam.c_str()), String::New(clobVal.c_str(), totalBytesRead));				
      				delete [] buffer;
      				break;
            }
          case OutParam::OCCIBLOB:
            {
              output->blobVal.open(oracle::occi::OCCI_LOB_READONLY);
              int lobLength = output->blobVal.length();
              oracle::occi::Stream* instream = output->blobVal.getStream(1,0);
              char *buffer = new char[lobLength];
              memset(buffer, 0, lobLength);
              instream->readBuffer(buffer, lobLength);
              output->blobVal.closeStream(instream);
              output->blobVal.close();

              // convert to V8 buffer
              uni::BufferType nodeBuff = node::Buffer::New(buffer, lobLength, RandomBytesFree, NULL);
              v8::Local<v8::Object> globalObj = v8::Context::GetCurrent()->Global();
              v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(v8::String::New("Buffer")));
              v8::Handle<v8::Value> constructorArgs[3] = { uni::BufferToHandle(nodeBuff), v8::Integer::New(lobLength), v8::Integer::New(0) };
              v8::Local<v8::Object> v8Buffer = bufferConstructor->NewInstance(3, constructorArgs);
              obj->Set(String::New(returnParam.c_str()), v8Buffer);
              delete [] buffer;
              break;
            }
          case OutParam::OCCIDATE:
            obj->Set(String::New(returnParam.c_str()), OracleDateToV8Date(&output->dateVal));
            break;
          case OutParam::OCCITIMESTAMP:
            obj->Set(String::New(returnParam.c_str()), OracleTimestampToV8Date(&output->timestampVal));
            break;
          case OutParam::OCCINUMBER:
            obj->Set(String::New(returnParam.c_str()), Number::New(output->numberVal));
            break;
          default:
            {
              ostringstream oss;
              oss << "Unknown OutParam type: " << output->type;
              baton->error = new std::string(oss.str());
              goto failed;
            }
        }
      }
      argv[1] = obj;
    }
  }
}

void Connection::setConnection(oracle::occi::Environment* environment, oracle::occi::Connection* connection) {
  m_environment = environment;
  m_connection = connection;
}

uni::CallbackType Connection::ExecuteSync(const uni::FunctionCallbackInfo& args) {
  UNI_SCOPE(scope);
  Connection* connection = ObjectWrap::Unwrap<Connection>(args.This());

  REQ_STRING_ARG(0, sql);
  REQ_ARRAY_ARG(1, values);

  String::Utf8Value sqlVal(sql);

  ExecuteBaton* baton = new ExecuteBaton(connection, *sqlVal, &values, NULL);
  if (baton->error) {
    Local<String> message = String::New(baton->error->c_str());
    delete baton;
    UNI_THROW(Exception::Error(message));
  }

  uv_work_t* req = new uv_work_t();
  req->data = baton;

  baton->connection->Ref();
  EIO_Execute(req);
  baton->connection->Unref();
  Handle<Value> argv[2];
  handleResult(baton, argv);

  if(baton->error) {
    delete baton;
    UNI_THROW(argv[0]);
  }

  delete baton;
  UNI_RETURN(scope, args, argv[1]);
}
