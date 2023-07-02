#pragma once

#include <algorithm>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>

extern "C" {
#include "aiger.h"
}



class Graph {

protected:

  enum class node_type_t {FALSE, INPUT, STATEVAR,AND, NOT, BAD, CONSTRAINT};
  std::unordered_map<size_t, node_type_t> nodes;
  std::vector<std::pair<size_t, size_t>> edges;

public:

  Graph(aiger * aig);

  size_t add_node(node_type_t t) {
    auto id = nodes.size();
    nodes[id] = t;
    return id;
  }

  void add_edge(size_t from, size_t to) {
    edges.push_back({from, to});
  }

  void add_edge_lit(size_t from, size_t to);
  void dump(const std::string & fname) const;

};


