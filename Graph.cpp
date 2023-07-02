#include "Graph.h"

#include <iostream>
#include <fstream>
#include <assert.h>

// lit
// var

void Graph::dump(const std::string & fname) const {
  std::ofstream fout(fname);
  if(fout.is_open()) {
    fout << nodes.size() << " " << edges.size() << "\n";

    for(const auto & n : nodes)
      fout << n.first << " " << (size_t)(n.second) << "\n";
    for(const auto & e : edges)
      fout << e.first << " " << e.second << "\n";
  }
}


// from is a literal, to is variable !!!
void Graph::add_edge_lit(size_t from, size_t to) {
  if (from & 1) { // neg
    auto n = add_node(node_type_t::NOT);
    add_edge(from/2, n);
    add_edge(n, to);
      // add_edge (from/2, )
  } else
    add_edge(from/2, to);
}


Graph::Graph(aiger * aig) {
    // build graph?
  // 1 num_inputs 

  size_t n_nodes = 1 + aig->num_inputs + aig->num_latches;
  std::vector<std::pair<size_t,size_t>> edges;
  std::vector<node_type_t> node_types;

  add_node(node_type_t::FALSE);
  for (size_t i = 0; i < aig->num_inputs; ++i) {
    add_node(node_type_t::INPUT);
  }
    
  for (size_t i = 0; i < aig->num_latches; ++i) {
    add_node(node_type_t::STATEVAR);
  }

  for (size_t i = 0; i < aig->num_ands; ++i) {
    // 1. create a representative
    auto a = add_node(node_type_t::AND);
    add_edge_lit(aig->ands[i].rhs0, a);
    add_edge_lit(aig->ands[i].rhs1, a);
  }

  // acquire latches' initial states and next-state functions
  for (size_t i = 0; i < aig->num_latches; ++i) {
    add_edge_lit(aig->latches[i].next, 1+aig->num_inputs+i);
    unsigned int r = aig->latches[i].reset;
    assert(r==0);
  }

  assert (aig->num_constraints == 0);
  assert(aig->num_bad == 0);

  for (size_t i = 0; i < aig->num_outputs; ++i) {
    auto nb = add_node(node_type_t::BAD);
    add_edge_lit(aig->outputs[i].lit, nb);
  }


}

 