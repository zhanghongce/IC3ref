#include "ts.h"
#include "clausebuf.h"

#include "assert.h"

// create the term for a clause
smt::Term get_clause_term(const std::vector<int> & clause, TransitionSystem & ts)  {
  smt::TermVec list_or_literal;
  for (const auto lit : clause) {
    list_or_literal.push_back(ts.get_term(lit));
  }
  assert (!list_or_literal.empty());
  if (list_or_literal.size() ==  1)
    return list_or_literal.at(0);
  return ts.solver()->make_term(smt::PrimOp::And, list_or_literal);
}

// return true if prop is not true for init or F1
bool is_ts_trivially_unsafe(TransitionSystem & ts) {
  auto init = ts.init();
  auto trans = ts.trans();
  auto & solver = ts.get_solver();
  auto bad =  ts.bad();
  solver->push();
    solver->assert_formula(init);
    solver->assert_formula(bad);
    auto r1 = solver->check_sat();
  solver->pop();
  if (r1.is_sat())
    return true;

  solver->push();
    solver->assert_formula(init);
    solver->assert_formula(trans);
    solver->assert_formula(ts.next(bad));
    auto r2 = solver->check_sat();
  solver->pop();
  if (r2.is_sat())
    return true;
  return false;
}

void filter_clauses(const ClauseBuf & in, ClauseBuf & out, TransitionSystem & ts) {
  auto init = ts.init();
  auto trans = ts.trans();
  auto & solver = ts.get_solver();
  auto prop = solver->make_term(smt::PrimOp::Not, ts.bad());

  // init /\ \neg c               unsat
  // init /\ T /\ \neg (next(p))  unsat
  for (const auto & cl : in.clauses) {
    auto clause_term = get_clause_term(cl, ts);
    solver->push();
      solver->assert_formula(init);
      solver->assert_formula(prop);
      solver->assert_formula( clause_term );
      auto r1 = solver->check_sat();
    solver->pop();
    if (r1.is_sat())
      continue;

    solver->push();
      solver->assert_formula(init);
      solver->assert_formula(trans);
      solver->assert_formula(prop);
      solver->assert_formula(ts.next(prop));
      solver->assert_formula(ts.next( clause_term)) ;
      auto r2 = solver->check_sat();
    solver->pop();
    if (r2.is_sat())
      continue;

    // not unsat
    out.clauses.push_back(cl);
  }
}

