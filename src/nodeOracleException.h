
#ifndef _nodeOracleException_h_
#define _nodeOracleException_h_

#include <string>

class NodeOracleException {
public:
  NodeOracleException(std::string message) : m_message(message) {
  }
  std::string getMessage() { return m_message; }

private:
  std::string m_message;
};

#endif
