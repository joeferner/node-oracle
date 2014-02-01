
#ifndef _reader_baton_h_
#define _reader_baton_h_

#include "connection.h"
#include "executeBaton.h"

class ReaderBaton : public ExecuteBaton {
public:
  ReaderBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values) : ExecuteBaton(connection, sql, values, NULL) {
    stmt = NULL;
    rs = NULL;
    done = false;
    busy = false;
  }
  ~ReaderBaton() {
    ResetStatement();
  }

  void ResetStatement() {
	  if(stmt && rs) {
      stmt->closeResultSet(rs);
      rs = NULL;
    }
    if(stmt) {
      if(connection->getConnection()) {
         connection->getConnection()->terminateStatement(stmt);
      }
      stmt = NULL;
    }
  }

  oracle::occi::Statement* stmt;
  oracle::occi::ResultSet* rs;
  int count;
  bool done;
  bool busy;
};

#endif
