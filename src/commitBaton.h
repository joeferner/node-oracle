
#ifndef _commit_baton_h_
#define _commit_baton_h_

#include "connection.h"

class CommitBaton {
public:
  CommitBaton(Connection* connection, v8::Handle<v8::Function>* callback) {
    this->connection = connection;
    uni::Reset(this->callback, *callback);
  }
  ~CommitBaton() {
    callback.Dispose();
  }

  Connection *connection;
  v8::Persistent<v8::Function> callback;
};

#endif
