#include "ts.h"
#include "clausebuf.h"

#include "assert.h"

#include <algorithm>

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


  bool ctiModel::equal(const ctiModel & other)  const {
    if (vars.size() != other.vars.size())
      return false;
    for (auto var_it = vars.begin(), 
              val_it = vals.begin(),
              ovar_it = other.vars.begin(),
              oval_it = other.vals.begin(); 
          var_it != vars.end() && ovar_it != other.vars.end();
          ++var_it, ++val_it, ++ovar_it, ++oval_it  ) {
      if (*var_it != *ovar_it || (*val_it)->to_string() != (*oval_it)->to_string() )
        return false;
    }
    return true;
  }


  // similar to ternary simulation
  void ctiModel::reduce_vars(smt::SmtSolver & old, smt::UnsatCoreReducer & reducer, const smt::Term & prop) {

    smt::TermVec assumps;
    smt::TermVec remaining,remaining_final;
    for (auto vpos = vars.begin(), valpos = vals.begin(); vpos != vars.end(); ++ vpos, ++valpos) {
      assumps.push_back(old->make_term(smt::Equal, *vpos, *valpos));
    }
    bool r1 = reducer.reduce_assump_unsatcore(prop, assumps, remaining);
    assert(r1);
    bool r2 = reducer.linear_reduce_assump_unsatcore(prop, remaining, remaining_final);
    assert(r2);

    std::vector<size_t> vidx_to_remove;
    size_t idx1 = 0;
    for (size_t idx2 = 0; idx1 < assumps.size() && idx2 < remaining_final.size(); ++ idx1) {
      if(remaining_final.at(idx2) == assumps.at(idx1)) {
        idx2 ++;
      } else {
        vidx_to_remove.push_back(idx1);
      }
    }
    for (;idx1 < assumps.size();++idx1) {
      // removing the rest
      vidx_to_remove.push_back(idx1);
    }

    for (std::size_t i = 0; i < vidx_to_remove.size(); ++i) {
        vidx_to_remove[i] -= i;
    }
    for (const int index : vidx_to_remove) {
        assert (index >= 0 && index < vars.size());
        vars.erase(vars.begin() + index);
        vals.erase(vals.begin() + index);   
    }
  }

  ctiModel::ctiModel(smt::SmtSolver & s, const smt::TermVec & v) {
    for(const auto & var : v) {
      vars.push_back(var);
      vals.push_back(s->get_value(var));
    }
  }

  smt::Term ctiModel::get_exp(smt::SmtSolver & s) const {
    smt::Term ret;
    for (auto vpos = vars.begin(), valpos = vals.begin(); vpos != vars.end(); ++ vpos, ++valpos) {
      if (ret == nullptr)
        ret = s->make_term(smt::Equal, *vpos, *valpos);
      else
        ret = s->make_term(smt::And, ret, s->make_term(smt::Equal, *vpos, *valpos));
    }
    return ret;
  }

  std::string ctiModel::get_vals() const {
    std::string ret;
    for (const auto & v : vals) {
      ret += std::to_string(v->to_int()) + "\t";
    }
    return ret;
  }

  std::string ctiModel::get_vars() const {
    std::string ret;
    for (const auto & v : vars) {
      ret += v->to_string() + "\t";
    }
    return ret;
  }



// P /\ T /\ not P
int sample_cti(TransitionSystem & ts, int sample_count, std::vector<ctiModel> & out) {
  auto & solver = ts.get_solver();
  auto bad = ts.bad();
  auto bad_next = solver->substitute(bad, ts.state_updates()); // P[substitute with current]
  auto not_bad_next = solver->make_term(smt::Not, bad_next);

  auto reducer_solver = smt::BoolectorSolverFactory::create(false);
  smt::UnsatCoreReducer  reducer(reducer_solver);

  smt::UnorderedTermSet vars;
  smt::get_free_symbols(bad, vars);
  smt::get_free_symbols(bad_next, vars);
  smt::TermVec vars_vec(vars.begin(), vars.end());
  std::sort(vars_vec.begin(), vars_vec.end(), [](const smt::Term &l, const smt::Term &r) {
    return l->to_string() < r->to_string();
  } );

  solver->push();
    solver->assert_formula(solver->make_term(smt::PrimOp::Not, bad));
    solver->assert_formula(bad_next);
  
  int cnt = 0;
  while(cnt < sample_count) {
    auto r1 = solver->check_sat();
    if (r1.is_sat()) {
      // extract model
      ctiModel m(solver, vars_vec);
      m.reduce_vars(solver, reducer, not_bad_next);
      std::cout << m.get_vars() << std::endl;
      std::cout << m.get_vals() << std::endl;
      solver->assert_formula(solver->make_term(smt::Not,m.get_exp(solver)));
      out.push_back(m);
    } else
      break;
    cnt ++;
  }
  solver->pop();
  return cnt;
}
