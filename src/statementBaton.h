
#ifndef _statement_baton_h_
#define _statement_baton_h_

#include "connection.h"
#include "executeBaton.h"

class StatementBaton : public ExecuteBaton {
public:
  StatementBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values) : ExecuteBaton(connection, sql, values, NULL) {
    stmt = NULL;
    done = false;
    busy = false;
  }
  ~StatementBaton() {
    ResetStatement();
  }

  void ResetStatement() {
    if (stmt) {
      if (connection->getConnection()) {
         connection->getConnection()->terminateStatement(stmt);
      }
      stmt = NULL;
    }
  }

  oracle::occi::Statement* stmt;
  bool done;
  bool busy;
};

#endif
