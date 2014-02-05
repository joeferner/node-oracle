
#ifndef _reader_baton_h_
#define _reader_baton_h_

#include "connection.h"
#include "statementBaton.h"

class ReaderBaton : public StatementBaton {
public:
  ReaderBaton(Connection* connection, const char* sql, v8::Local<v8::Array>* values) : StatementBaton(connection, sql, values) {
    stmt = NULL;
    rs = NULL;
    done = false;
    busy = false;
  }
  ~ReaderBaton() {
    ResetStatement();
  }

  void ResetStatement() {
	  if (stmt && rs) {
      stmt->closeResultSet(rs);
      rs = NULL;
    }
    StatementBaton::ResetStatement();
  }

  oracle::occi::ResultSet* rs;
  int count;
};

#endif
