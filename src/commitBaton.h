
#ifndef _commit_baton_h_
#define _commit_baton_h_

#include "connection.h"

class CommitBaton {
public:
  CommitBaton(Connection* connection, v8::Handle<v8::Function>* callback) {
    this->connection = connection;
    this->callback = Persistent<Function>::New(*callback);
  }
  ~CommitBaton() {
    callback.Dispose();
  }

  Connection *connection;
  v8::Persistent<v8::Function> callback;
};

#endif
