#pragma once

#include "smt-switch/smt.h"
#include "smt-switch/utils.h"
#include "smt-switch/boolector_factory.h"


extern "C" {
#include "aiger.h"
}

class TransitionSystem;
class ClauseBuf;

class PonoException : public std::exception {
  public:
    const std::string msg;
    PonoException(const std::string & m) : msg(m) { }
};


struct ctiModel {
  std::vector<smt::Term> vars;
  std::vector<smt::Term> vals;
  bool equal(const ctiModel & other)  const ;
  void reduce_vars(smt::SmtSolver & old, smt::UnsatCoreReducer & reducer_solver, const smt::Term & prop) ;
  ctiModel(smt::SmtSolver & s, const smt::TermVec & v);
  smt::Term get_exp(smt::SmtSolver & s) const ;
  std::string get_vals() const ;
  std::string get_vars() const ;
};
int sample_cti(TransitionSystem & ts, int sample_count, std::vector<ctiModel> & out);

void filter_clauses(const ClauseBuf & in, ClauseBuf & out, TransitionSystem & ts);
bool is_ts_trivially_unsafe(TransitionSystem & ts) ;

class TransitionSystem
{
 public:

  TransitionSystem(aiger * aig, unsigned int propertyIndex) ;
  

  virtual ~TransitionSystem(){};


  /* Sets initial states to the provided formula
   * @param init the new initial state constraints
   */
  void set_init(const smt::Term & init);

  /* Add to the initial state constraints
   * @param constraint new constraint on initial states
   */
  void constrain_init(const smt::Term & constraint);

  /* Set the transition function of a state variable
   *   val is constrained to only use current state variables
   * Represents a functional update
   * @param state the state variable you are updating
   * @param val the value it should get
   * Throws a PonoException if:
   *  1) state is not a state variable
   *  2) val contains any next state variables (assign next is for functional
   * assignment)
   *  3) state has already been assigned a next state update
   */
  void assign_next(const smt::Term & state, const smt::Term & val);

  /* Add an invariant constraint to the system
   * This is enforced over all time
   * Specifically, it adds the constraint over both current and next variables
   * @param constraint the boolean constraint term to add (should contain only
   * state variables)
   */
  void add_invar(const smt::Term & constraint);

  /** Add a constraint over inputs
   * @param constraint to add (should not contain any next-state variables)
   */
  void constrain_inputs(const smt::Term & constraint);

  /** Convenience function for adding a constraint to the system
   * if the constraint only has current states, equivalent to add_invar
   * if there are only current states and inputs, equivalent to
   *   constrain_inputs
   *  @param constraint the constraint to add
   *  @param to_init_and_next whether it should be added to init and
   *         over next state variables as well
   *         (if it only contains state variables)
   * throws an exception if it has next states (should go in trans)
   */
  void add_constraint(const smt::Term & constraint,
                      bool to_init_and_next = true);

  /* Gives a term a name
   *   This can be used to track particular values in a witness
   * @param name the (unique) name to give the term
   * @param t the term to name
   *
   * Throws an exception if the name has already been used
   *  Note: giving multiple names to the same term is allowed
   */
  void name_term(const std::string name, const smt::Term & t);

  /* Create an input of a given sort
   * @param name the name of the input
   * @param sort the sort of the input
   * @return the input term
   */
  smt::Term make_inputvar(const std::string name, const smt::Sort & sort);

  /* Create an state of a given sort
   * @param name the name of the state
   * @param sort the sort of the state
   * @return the current state variable
   *
   * Can get next state var with next(const smt::Term t)
   */
  smt::Term make_statevar(const std::string name, const smt::Sort & sort);

  /* Map all next state variables to current state variables in the term
   * @param t the term to map
   * @return the term with all current state variables
   */
  smt::Term curr(const smt::Term & term) const;

  /* Map all current state variables to next state variables in the term
   * @param t the term to map
   * @return the term with all next state variables
   */
  smt::Term next(const smt::Term & term) const;

  /* @param sv the state variable to check
   * @return true if sv is a current state variable
   *
   * Returns false for any other term
   */
  bool is_curr_var(const smt::Term & sv) const;

  /* @param sv the state variable to check
   * @return true if sv is a next state variable
   *
   * Returns false for any other term
   */
  bool is_next_var(const smt::Term & sv) const;

  /* @param t the term to check
   * @return true if t is an input variable
   *
   * Returns false for any other term
   */
  bool is_input_var(const smt::Term & t) const;

  /** Looks for a representative name for a term
   *  It searches for a name that was assigned to the term
   *  if it cannot be found, then it just returns the
   *  smt-lib to_string
   *  @param t the term to look for a name for
   *  @return a string for the term
   */
  std::string get_name(const smt::Term & t) const;

  /** Find a term by name in the transition system.
   *  searches current and next state variables, inputs,
   *  and named terms.
   *  Throws a PonoException if there is no matching term.
   *
   *  @param name the name to look for
   *  @return the matching term if found
   */
  smt::Term lookup(std::string name) const;

  /** Adds a state variable with the given next state
   *  @param cv the current state variable
   *  @param nv the next state variable
   */
  void add_statevar(const smt::Term & cv, const smt::Term & nv);

  /** Adds an input variable
   *  @param v the input variable
   */
  void add_inputvar(const smt::Term & v, const smt::Term & nv);

  // getters
  /* Returns const reference to solver */
  const smt::SmtSolver & solver() const { return solver_; };

  /* Gets a non-const reference to the solver */
  smt::SmtSolver & get_solver() { return solver_; };

  const smt::UnorderedTermSet & statevars() const { return statevars_; };

  const smt::UnorderedTermSet & inputvars() const { return inputvars_; };

  /* Returns the initial state constraints
   * @return a boolean term constraining the initial state
   */
  smt::Term init() const { return init_; };

  /* Returns the transition relation
   * @return a boolean term representing the transition relation
   */
  smt::Term trans() const { return trans_; };

  /* Returns the negated property to check
   * @return a boolean term representing the negated property to check
   */
  smt::Term bad() const { return bad_; };

  /* Returns the next state updates
   * @return a map of functional next state updates
   */
  const smt::UnorderedTermMap & state_updates() const
  {
    return state_updates_;
  };

  /* @return the named terms mapping */
  const std::unordered_map<std::string, smt::Term> & named_terms() const
  {
    return named_terms_;
  };

  /** @return the constraints of the system
   *  Note: these do not include next-state variable updates or initial state
   * constraints
   *  Returned as a vector of pairs where for each element:
   *    first: is the constraint
   *    second: is a boolean saying whether it can be added over init / next
   * states This allows you to re-add the constraints by unpacking them and
   * passing to add_constraint, e.g. add_constraint(e.first, e.second)
   */
  const std::vector<smt::Term> & constraints() const
  {
    return constraints_;
  };

  /* Returns true iff all the symbols in the formula are current states */
  bool only_curr(const smt::Term & term) const;

  /* Returns true iff all the symbols in the formula are inputs and current
   * states */
  bool no_next(const smt::Term & term) const;


  /* Returns the term for a literal */
  smt::Term get_term(size_t literal) {
    if(literal & 1)  // literal & 2 == 1
      return solver_->make_term(smt::Op(smt::PrimOp::Not), term_map.at(literal/2));
    return term_map.at(literal/2);
  };

 protected:
  // solver
  smt::SmtSolver solver_;

  // initial state constraint
  smt::Term init_;

  // transition relation (functional in this class)
  smt::Term trans_;

  // negated property
  smt::Term bad_;

  // system state variables
  smt::UnorderedTermSet statevars_;

  // set of next state variables
  smt::UnorderedTermSet next_statevars_;

  // system inputs
  smt::UnorderedTermSet inputvars_;

  // system inputs (primed)
  smt::UnorderedTermSet next_inputvars_;

  // mapping from names to terms
  std::unordered_map<std::string, smt::Term> named_terms_;

  // mapping from terms to a representative name
  // because a term can have multiple names
  std::unordered_map<smt::Term, std::string> term_to_name_;

  // next state update function
  smt::UnorderedTermMap state_updates_;

  // maps states and inputs variables to next versions
  // note: the next state variables are only used
  //       on the left hand side of equalities in
  //       trans for functional transition systems
  smt::UnorderedTermMap next_map_;

  // maps next back to curr
  smt::UnorderedTermMap curr_map_;

  std::vector<smt::Term> constraints_;
  ///< constraints added via
  ///< add_invar/constrain_inputs/add_constraint
  
  // aig number to terms
  std::vector<smt::Term> term_map;

  typedef std::vector<const smt::UnorderedTermSet *> UnorderedTermSetPtrVec;

  /** Returns true iff all symbols in term are present in at least one of the
   * term sets
   *  @param term the term to check
   *  @param term_sets a vector of sets to check membership for each symbol in
   * term
   *  @return true iff all symbols in term are in at least one of the term sets
   */
  bool contains(const smt::Term & term, UnorderedTermSetPtrVec term_sets) const;
};

