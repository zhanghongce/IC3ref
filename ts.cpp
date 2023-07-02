#include "ts.h"

#include <iostream>
#include <fstream>
#include <assert.h>
#include <functional>

#include "assert.h"
#include "smt-switch/substitution_walker.h"
#include "smt-switch/utils.h"

using namespace smt;
using namespace std;



TransitionSystem::TransitionSystem(aiger * aig, unsigned int propertyIndex) : 
  solver_(smt::BoolectorSolverFactory::create(true)),
  init_(solver_->make_term(true)),
  trans_(solver_->make_term(true))
{
  solver_->set_opt("incremental", "true");
  solver_->set_opt("produce-models", "true");
  solver_->set_opt("produce-unsat-assumptions", "true");
  // build graph?
  // 1 num_inputs 

  term_map.push_back(solver_->make_term(false)); // 0 : false
  auto bool_sort = solver_->make_sort(smt::SortKind::BOOL);
  auto AND_op = smt::Op(smt::PrimOp::And);
  auto EQUAL_op = smt::Op(smt::PrimOp::Equal);


  for (size_t i = 0; i < aig->num_inputs; ++i) {
    std::string input_name = aig->inputs[i].name ? std::string(aig->inputs[i].name) : "input" + std::to_string(i) ;
    term_map.push_back( make_inputvar(input_name, bool_sort) );
  }
    
  for (size_t i = 0; i < aig->num_latches; ++i) {
    std::string latch_name = aig->latches[i].name ? std::string(aig->latches[i].name) : "state" + std::to_string(i) ;
    term_map.push_back( make_statevar(latch_name, bool_sort) );
  }

  for (size_t i = 0; i < aig->num_ands; ++i) {
    // 1. create a representative
    term_map.push_back(
      solver_->make_term(
        AND_op, 
        get_term(aig->ands[i].rhs0),
        get_term(aig->ands[i].rhs1)));
  }

  // acquire latches' initial states and next-state functions
  for (size_t i = 0; i < aig->num_latches; ++i) {
    auto statevar = term_map.at(1+aig->num_inputs+i);
    auto next_expr = get_term(aig->latches[i].next);

    assign_next(statevar, next_expr);

    unsigned int r = aig->latches[i].reset;
    assert(r==0);
    constrain_init(solver_->make_term(EQUAL_op,statevar, term_map.at(0))); // initially it is 0
  }

  assert (aig->num_constraints == 0); // no additional constraints
  assert(aig->num_bad == 0); // no bad (only 1 output)
  assert(aig->num_outputs >= 1);
  assert(propertyIndex < aig->num_outputs);

  bad_ = get_term(aig->outputs[propertyIndex].lit);

  // check if any states have no next, changed to input
  decltype(statevars_) sv_to_remove;
  for (const auto & n : statevars_) {
    if (state_updates_.find(n) == state_updates_.end()) {
      // move to input
      sv_to_remove.emplace(n);
    }
  }
  for (const auto & sv : sv_to_remove) {
    auto nv = next(sv);
    statevars_.erase(sv);
    next_statevars_.erase(nv);
    inputvars_.emplace(sv);
    next_inputvars_.emplace(nv);
  }

} // end of constructor


void TransitionSystem::set_init(const Term & init)
{
  // TODO: only do this check in debug mode
  if (!only_curr(init)) {
    throw PonoException(
        "Initial state constraints should only use current state variables");
  }

  init_ = init;
}

void TransitionSystem::constrain_init(const Term & constraint)
{
  // TODO: Only do this check in debug mode
  if (!only_curr(constraint)) {
    throw PonoException(
        "Initial state constraints should only use current state variables");
  }
  init_ = solver_->make_term(And, init_, constraint);
}

void TransitionSystem::assign_next(const Term & state, const Term & val)
{
  // TODO: only do this check in debug mode
  if (statevars_.find(state) == statevars_.end()) {
    throw PonoException("Unknown state variable");
  }

  if (!no_next(val)) {
    throw PonoException(
        "Got a symbolic that is not a current state or input variable in RHS "
        "of functional assignment");
  }

  if (state_updates_.find(state) != state_updates_.end()) {
    throw PonoException("State variable " + state->to_string()
                        + " already has next-state logic assigned.");
  }

  state_updates_[state] = val;
  trans_ = solver_->make_term(
      And, trans_, solver_->make_term(Equal, next_map_.at(state), val));
}

void TransitionSystem::add_invar(const Term & constraint)
{
  // TODO: only check this in debug mode
  if (only_curr(constraint)) {
    init_ = solver_->make_term(And, init_, constraint);
    trans_ = solver_->make_term(And, trans_, constraint);
    Term next_constraint = solver_->substitute(constraint, next_map_);
    // add the next-state version
    trans_ = solver_->make_term(And, trans_, next_constraint);
    constraints_.push_back(constraint);
  } else {
    throw PonoException("Invariants should be over current states only.");
  }
}

void TransitionSystem::constrain_inputs(const Term & constraint)
{
  if (no_next(constraint)) {
    trans_ = solver_->make_term(And, trans_, constraint);
    constraints_.push_back(constraint);
  } else {
    throw PonoException("Cannot have next-states in an input constraint.");
  }
}

void TransitionSystem::add_constraint(const Term & constraint,
                                      bool to_init_and_next)
{
  if (only_curr(constraint)) {
    trans_ = solver_->make_term(And, trans_, constraint);

    if (to_init_and_next) {
      init_ = solver_->make_term(And, init_, constraint);
      Term next_constraint = solver_->substitute(constraint, next_map_);
      trans_ = solver_->make_term(And, trans_, next_constraint);
    }
    constraints_.push_back(constraint);
  } else if (no_next(constraint)) {
    trans_ = solver_->make_term(And, trans_, constraint);
    constraints_.push_back(constraint);
  } else {
    throw PonoException("Constraint cannot have next states");
  }
}

void TransitionSystem::name_term(const string name, const Term & t)
{
  auto it = named_terms_.find(name);
  if (it != named_terms_.end() && t != it->second) {
    throw PonoException("Name " + name + " has already been used.");
  }
  named_terms_[name] = t;
  // save this name as a representative (might overwrite)
  term_to_name_[t] = name;
}

Term TransitionSystem::make_inputvar(const string name, const Sort & sort)
{
  Term input = solver_->make_symbol(name, sort);
  Term next_input = solver_->make_symbol(name + ".next", sort);
  add_inputvar(input, next_input);
  return input;
}

Term TransitionSystem::make_statevar(const string name, const Sort & sort)
{
  Term state = solver_->make_symbol(name, sort);
  Term next_state = solver_->make_symbol(name + ".next", sort);
  add_statevar(state, next_state);
  return state;
}

Term TransitionSystem::curr(const Term & term) const
{
  return solver_->substitute(term, curr_map_);
}

Term TransitionSystem::next(const Term & term) const
{
  if (next_map_.find(term) != next_map_.end()) {
    return next_map_.at(term);
  }
  return solver_->substitute(term, next_map_);
}

bool TransitionSystem::is_curr_var(const Term & sv) const
{
  return (statevars_.find(sv) != statevars_.end());
}

bool TransitionSystem::is_next_var(const Term & sv) const
{
  return (next_statevars_.find(sv) != next_statevars_.end());
}

bool TransitionSystem::is_input_var(const Term & sv) const
{
  return (inputvars_.find(sv) != inputvars_.end());
}

std::string TransitionSystem::get_name(const Term & t) const
{
  const auto & it = term_to_name_.find(t);
  if (it != term_to_name_.end()) {
    return it->second;
  }
  return t->to_string();
}

smt::Term TransitionSystem::lookup(std::string name) const
{
  const auto & it = named_terms_.find(name);
  if (it == named_terms_.end()) {
    throw PonoException("Could not find term named: " + name);
  }
  return it->second;
}

void TransitionSystem::add_statevar(const Term & cv, const Term & nv)
{
  // TODO: this runs even if called from make_statevar
  //       could refactor entirely, or just pass a flag
  //       saying whether to check these things or not

  if (statevars_.find(cv) != statevars_.end()) {
    throw PonoException("Cannot redeclare a state variable");
  }

  if (next_statevars_.find(nv) != next_statevars_.end()) {
    throw PonoException("Cannot redeclare a state variable");
  }

  if (next_statevars_.find(cv) != next_statevars_.end()) {
    throw PonoException(
        "Cannot use an existing next state variable as a current state var");
  }

  if (statevars_.find(nv) != statevars_.end()) {
    throw PonoException(
        "Cannot use an existing state variable as a next state var");
  }

  // if using an input variable, remove from set
  // will be a state variable now
  if (inputvars_.find(cv) != inputvars_.end()) {
    bool success = inputvars_.erase(cv);
    assert(success);
  }

  if (inputvars_.find(nv) != inputvars_.end()) {
    bool success = inputvars_.erase(nv);
    assert(success);
  }

  statevars_.insert(cv);
  next_statevars_.insert(nv);
  next_map_[cv] = nv;
  curr_map_[nv] = cv;
  // automatically include in named_terms
  name_term(cv->to_string(), cv);
  name_term(nv->to_string(), nv);
}

void TransitionSystem::add_inputvar(const smt::Term & v, const smt::Term & nv)
{
  // TODO: this check is running even when used by make_inputvar
  //       could refactor entirely or just pass a boolean saying whether or not
  //       to check these things
  if (statevars_.find(v) != statevars_.end()
      || next_statevars_.find(v) != next_statevars_.end()
      || inputvars_.find(v) != inputvars_.end()) {
    throw PonoException(
        "Cannot reuse an existing variable as an input variable");
  }

  inputvars_.insert(v);
  next_inputvars_.insert(v);
  // automatically include in named_terms
  name_term(v->to_string(), v);
  name_term(nv->to_string(), nv);
  
  next_map_[v] = nv;
  curr_map_[nv] = v;
}

bool TransitionSystem::contains(const Term & term,
                                UnorderedTermSetPtrVec term_sets) const
{
  UnorderedTermSet visited;
  TermVec to_visit{ term };
  Term t;
  while (to_visit.size()) {
    t = to_visit.back();
    to_visit.pop_back();

    if (visited.find(t) != visited.end()) {
      // cache hit
      continue;
    }

    if (t->is_symbolic_const()) {
      bool in_atleast_one = false;
      for (const auto & ts : term_sets) {
        if (ts->find(t) != ts->end()) {
          in_atleast_one = true;
          break;
        }
      }

      if (!in_atleast_one) {
        return false;
      }
    }

    visited.insert(t);
    for (const auto & c : t) {
      to_visit.push_back(c);
    }
  }

  return true;
}

bool TransitionSystem::only_curr(const Term & term) const
{
  return contains(term, UnorderedTermSetPtrVec{ &statevars_ });
}

bool TransitionSystem::no_next(const Term & term) const
{
  return contains(term, UnorderedTermSetPtrVec{ &statevars_, &inputvars_ });
}

