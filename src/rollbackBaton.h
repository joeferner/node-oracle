
#ifndef _rollback_baton_h_
#define _rollback_baton_h_

#include "connection.h"

class RollbackBaton {
public:
  RollbackBaton(Connection* connection, v8::Handle<v8::Function>* callback) {
    this->connection = connection;
    uni::Reset(this->callback, *callback);
  }
  ~RollbackBaton() {
    callback.Dispose();
  }

  Connection *connection;
  v8::Persistent<v8::Function> callback;
};

#endif
