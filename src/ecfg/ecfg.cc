#include "ecfg/ecfg.h"
#include "ecfg/ecfgsexpr.h"
#include "util/vector_output.h"
#include "hmm/singletmpl.h"
#include "hsm/rind_em_matrix.h"

#define ECFG_default_terminal "Terminal_"

ECFG_matrix_set::ECFG_matrix_set (const ECFG_matrix_set& ems)
  : alphabet (ems.alphabet)
{
  for_const_contents (vector<ECFG_chain>, ems.chain, ec)
    {
      ECFG_chain& new_chain = chain[add_matrix (ec->word_len, ec->type, ec->classes)];
      new_chain.state = ec->state;
      new_chain.class_labels = ec->class_labels;
      new_chain.classes = ec->classes;
      if (ec->matrix)  // avoid null pointer errors for lineage-dependent models
	{
	  (EM_matrix_params&) *new_chain.matrix = *ec->matrix;
	  new_chain.matrix->timepoint_res = ec->matrix->timepoint_res;
	}
    }
}

ECFG_matrix_set::ECFG_matrix_set (const Alphabet& alphabet)
  : alphabet (alphabet)
{ }

ECFG_matrix_set::~ECFG_matrix_set()
{
  for_contents (vector<ECFG_chain>, chain, c)
    if (c->matrix)  // avoid deleting null pointers (as may be found in lineage-dependent models)
      delete c->matrix;
}

int ECFG_matrix_set::add_matrix (int len, ECFG_enum::Update_policy type, int n_classes, double tres)
{
  // get new chain index
  const int new_chain_index = chain.size();

  // figure out number of states
  if (n_classes < 1)
    n_classes = 1;
  const int A = n_classes * observed_states_by_word_len (len);

  // create new chain
  ECFG_chain new_chain;
  new_chain.word_len = len;
  new_chain.classes = n_classes;
  new_chain.class_labels = vector<sstring> (n_classes, sstring (len, '?'));
  new_chain.class_row = "?";
  new_chain.type = type;
  new_chain.is_parametric = false;
  new_chain.matrix_funcs = (EM_matrix_funcs*) 0;
  for (int i = 0; i < len; ++i)
    {
      sstring state_label;
      state_label << ECFG_default_terminal << new_chain_index+1 << "_" << i+1;
      new_chain.state.push_back (state_label);
    }

  // create appropriate EM_matrix
  switch (type)
    {
    case Rev:
      new_chain.matrix = new EM_matrix (1, A, 1, 0, tres);
      break;

    case Rind:
      new_chain.matrix = new RIND_EM_matrix (1, A, 1, 0, tres);
      break;

    case Irrev:
      new_chain.matrix = new Irrev_EM_matrix (1, A, 1, 0, tres);
      break;

    case Parametric:
      {
	PFunc_EM_matrix* pfunc_matrix = new PFunc_EM_matrix (1, A, tres);
	new_chain.matrix = pfunc_matrix;
	new_chain.matrix_funcs = &pfunc_matrix->funcs;
	new_chain.is_parametric = true;
      }
      break;

    case Hybrid:
      new_chain.matrix = (EM_matrix_base*) 0;
      break;

    default:
      THROWEXPR ("Bad policy");
      break;
    }

  // add chain and return
  chain.push_back (new_chain);
  return new_chain_index;
}

int ECFG_matrix_set::total_states (int chain_idx) const
{
  return observed_states (chain_idx) * chain[chain_idx].classes;
}

int ECFG_matrix_set::observed_states (int chain_idx) const
{
  return observed_states_by_word_len (chain[chain_idx].word_len);
}

int ECFG_matrix_set::observed_states_by_word_len (int word_len) const
{
  int s = 1;
  for (int i = 0; i < word_len; ++i)
    s *= alphabet.size();
  return s;
}

// Do not change default initial values for ECFG_state_info - subclasses depend on these
ECFG_state_info::ECFG_state_info()
  : bifurc (false),
    ldest (UndefinedState),
    rdest (UndefinedState),
    matrix (-1),
    l_context (0),
    l_emit (0),
    r_emit (0),
    r_context (0),
    min_len (0),
    max_len (-1),
    infix (false),
    prefix (false),
    suffix (false),
    gaps_ok (true),
    wild_gaps (true),
    indels (false),
    link_extend (0.),
    link_end (1.),
    ins_rate (0.),
    del_rate (0.),
    has_parametric_indels (false),
    sum_state (false)
{ }

ECFG_state_info::ECFG_state_info (int lsize, int rsize)
  : bifurc (false),
    ldest (UndefinedState),
    rdest (UndefinedState),
    matrix (-1),
    l_context (0),
    l_emit (lsize),
    r_emit (rsize),
    r_context (0),
    mul (lsize + rsize, -1),
    comp (lsize + rsize, false),
    min_len (lsize + rsize),
    max_len (-1),
    infix (false),
    prefix (false),
    suffix (false),
    gaps_ok (true),
    wild_gaps (true),
    indels (false),
    link_extend (0.),
    link_end (1.),
    ins_rate (0.),
    del_rate (0.),
    has_parametric_indels (false),
    sum_state (false)
{ }

ECFG_state_info::ECFG_state_info (int l_context, int l_emit, int r_emit, int r_context)
  : bifurc (false),
    ldest (UndefinedState),
    rdest (UndefinedState),
    matrix (-1),
    l_context (l_context),
    l_emit (l_emit),
    r_emit (r_emit),
    r_context (r_context),
    mul (l_context + l_emit + r_emit + r_context, -1),
    comp (l_context + l_emit + r_emit + r_context, false),
    min_len (l_emit + r_emit),
    max_len (-1),
    infix (false),
    prefix (false),
    suffix (false),
    gaps_ok (true),
    wild_gaps (true),
    indels (false),
    link_extend (0.),
    link_end (1.),
    ins_rate (0.),
    del_rate (0.),
    has_parametric_indels (false),
    sum_state (false)
{ }

ECFG_null_state_info::ECFG_null_state_info()
  : ECFG_state_info (0, 0)
{ }

ECFG_emitl_state_info::ECFG_emitl_state_info (int matrix_idx)
  : ECFG_state_info (1, 0)
{
  matrix = matrix_idx;
  mul[0] = 1;
}

ECFG_emitr_state_info::ECFG_emitr_state_info (int matrix_idx)
  : ECFG_state_info (0, 1)
{
  matrix = matrix_idx;
  mul[0] = 1;
}

ECFG_emitlr_state_info::ECFG_emitlr_state_info (int matrix_idx, int alphabet_size, const char* tag)
  : ECFG_state_info (1, 1)
{
  matrix = matrix_idx;
  mul[0] = 1;
  mul[1] = alphabet_size;
  if (tag)
    annot [sstring (tag)] = sstring ("<>");
}

ECFG_bifurc_state_info::ECFG_bifurc_state_info (int l, int r)
  : ECFG_state_info (0, 0)
{
  bifurc = true;
  ldest = l;
  rdest = r;
}

void ECFG_scores::init_default_state_names()
{
  for (int s = 0; s < states(); ++s)
    state_info[s].name << ECFG_default_nonterminal << s+1;
}

bool ECFG_scores::is_left_regular() const
{
  for_const_contents (vector<ECFG_state_info>, state_info, info)
    if (info->r_context || info->r_emit || info->bifurc)
      return false;
  return true;
}

bool ECFG_scores::is_right_regular() const
{
  for_const_contents (vector<ECFG_state_info>, state_info, info)
    if (info->l_context || info->l_emit || info->bifurc)
      return false;
  return true;
}

void ECFG_scores::set_infix_len (int max_subseq_len)
{
  vector<sstring> warn_maxlen, warn_minlen, warn_minlen_infix;
  for_contents (vector<ECFG_state_info>, state_info, info)
    {
      if (info->infix)
	info->max_len = max_subseq_len;
      if (info->max_len >= 0 && info->min_len > info->max_len)
	warn_minlen.push_back (info->name);
      if (!info->prefix && !info->suffix && max_subseq_len >= 0)
	{
	  if (info->max_len > max_subseq_len)
	    warn_maxlen.push_back (info->name);
	  if (info->min_len > max_subseq_len)
	    warn_minlen_infix.push_back (info->name);
	}
    }

  if (warn_minlen.size())
    CLOGERR << "Warning -- in grammar "
	    << name
	    << ", the following states have '"
	    << EG_TRANSFORM_MINLEN
	    << "' greater than '"
	    << EG_TRANSFORM_MAXLEN
	    << "', and so will be inaccessible: "
	    << warn_minlen
	    << '\n';

  if (warn_maxlen.size())
    CLOGERR << "Warning -- in grammar "
	    << name
	    << ", the following states have '"
	    << EG_TRANSFORM_MAXLEN
	    << "' set to greater than the overriding maximum infix length of "
	    << max_subseq_len
	    << " specified on the command line, and yet are not designated as '"
	    << EG_TRANSFORM_PREFIX
	    << "' or '"
	    << EG_TRANSFORM_SUFFIX
	    << "': "
	    << warn_maxlen
	    << '\n';

  if (warn_minlen_infix.size())
    CLOGERR << "Warning -- in grammar "
	    << name
	    << ", the following states have '"
	    << EG_TRANSFORM_MINLEN
	    << "' set to greater than the overriding maximum infix length of "
	    << max_subseq_len
	    << " specified on the command line, and yet are not designated as '"
	    << EG_TRANSFORM_PREFIX
	    << "' or '"
	    << EG_TRANSFORM_SUFFIX
	    << "': "
	    << warn_minlen_infix
	    << '\n';
}

const ECFG_chain* ECFG_scores::first_single_pseudoterminal_chain() const
{
  for (int m = 0; m < (int) matrix_set.chain.size(); ++m)
    if (matrix_set.chain[m].word_len == 1)
      return &matrix_set.chain[m];
  return (ECFG_chain*) 0;
}


vector<int> ECFG_scores::nonemit_states_unsorted() const
{
  vector<int> result;
  for (int s = 0; s < states(); ++s)
    if (state_info[s].emit_size() == 0)
      result.push_back(s);
  return result;
}

vector<int> ECFG_scores::nonemit_states() const
{
  const vector<int> unsorted = nonemit_states_unsorted();
  ((ECFG_scores*)this)->add_fake_bifurcation_transitions();  // cast away const
  const vector<int> sorted = Transition_methods::topological_sort (*this, unsorted);
  ((ECFG_scores*)this)->remove_fake_bifurcation_transitions();  // cast away const
  if (CTAGGING(1,ECFG_TOPOLOGICAL_SORT))
    {
      CL << "Sorted null states:";
      for_const_contents (vector<int>, sorted, s)
	CL << " " << state_info[*s].name << '(' << *s << ')';
      CL << "\n";
    }
  return sorted;
}

vector<int> ECFG_scores::emit_states() const
{
  vector<int> result;
  for (int s = 0; s < states(); ++s)
    if (state_info[s].emit_size() > 0)
      result.push_back(s);
  return result;
}

vector<int> ECFG_scores::bifurc_states() const
{
  vector<int> result;
  for (int s = 0; s < states(); ++s)
    if (state_info[s].bifurc)
      result.push_back (s);
  return result;
}

vector<vector<int> > ECFG_scores::left_bifurc() const
{
  vector<vector<int> > result (states());
  const vector<int> bif_states = bifurc_states();
  for (int rdest = 0; rdest < states(); ++rdest)
    for_const_contents (vector<int>, bif_states, b)
      if (state_info[*b].rdest == rdest)
	result[rdest].push_back (*b);
  return result;
}

vector<vector<int> > ECFG_scores::right_bifurc() const
{
  vector<vector<int> > result (states());
  const vector<int> bif_states = bifurc_states();
  for (int ldest = 0; ldest < states(); ++ldest)
    for_const_contents (vector<int>, bif_states, b)
      if (state_info[*b].ldest == ldest)
	result[ldest].push_back (*b);
  return result;
}

void ECFG_scores::write (ostream& out) const
{
  for (int i = 0; i < (int) matrix_set.chain.size(); ++i)
    matrix_set.chain[i].matrix->write (out);
  for (int src = Start; src < states(); ++src)
    {
      for (int dest = 0; dest < states(); ++dest)
	out << transition (src, dest) << ' ';
      out << transition (src, End) << '\n';
    }
  for (int s = 0; s < states(); ++s)
    {
      const ECFG_state_info& info = state_info[s];
      if (info.indels)
	out << info.link_extend << " " << info.ins_rate << " " << info.del_rate << "\n";
    }
}

void ECFG_scores::read (istream& in)
{
  for (int i = 0; i < (int) matrix_set.chain.size(); ++i)
    matrix_set.chain[i].matrix->read (in);
  for (int src = Start; src < states(); ++src)
    {
      for (int dest = 0; dest < states(); ++dest)
	in >> transition (src, dest);
      in >> transition (src, End);
    }
  for (int s = 0; s < states(); ++s)
    {
      ECFG_state_info& info = state_info[s];
      if (info.indels)
	{
	  in >> info.link_extend >> info.ins_rate >> info.del_rate;
	  info.link_end = 1. - info.link_extend;
	}
    }
}

void ECFG_scores::annotate (Stockholm& stock, const ECFG_cell_score_map& annot) const
{
  // initialise all annotations
  for_const_contents (vector<ECFG_state_info>, state_info, info)
    for_const_contents (ECFG_state_info::ECFG_state_annotation, info->annot, a)
    {
      const sstring& tag = a->first;
      sstring& row_annot = stock.gc_annot[tag];
      row_annot = sstring (stock.columns(), ECFG_annotation_wildcard);
    }

  // loop over subseq->state mappings
  for_const_contents (ECFG_cell_score_map, annot, ss)
    {
      const Subseq_coords& subseq = ss->first.first;
      const ECFG_state_info& info = state_info[ss->first.second];

      for_const_contents (ECFG_state_info::ECFG_state_annotation, info.annot, a)
	{
	  const sstring& tag = a->first;
	  const sstring& val = a->second;
	  sstring& row_annot = stock.gc_annot[tag];

	  if (info.emit_size())  // emit state: mark up emitted columns with annotation string
	    {
	      for (int pos = 0; pos < info.l_emit; ++pos)
		row_annot[subseq.start + pos] = val[pos];
	      for (int pos = 0; pos < info.r_emit; ++pos)
		row_annot[subseq.end() - info.r_emit + pos] = val[info.l_emit + pos];
	    }
	  else if (info.sum_state)  // non-emit sum state: mark up entire subsequence with annotation char
	    for (int pos = 0; pos < subseq.len; ++pos)
	      row_annot[subseq.start + pos] = val[0];
	}
    }
}

void ECFG_scores::make_GFF (GFF_list& gff_list,
			    const ECFG_cell_score_map& annot,
			    const char* seqname,
			    ECFG_posterior_probability_calculator* pp_calc,
			    ECFG_inside_calculator* ins_calc) const
{
  // loop over subseq->state mappings
  for_const_contents (ECFG_cell_score_map, annot, ss)
    {
      const Subseq_coords& subseq = ss->first.first;
      const int state = ss->first.second;
      const Loge ll = ss->second;

      const ECFG_state_info& info = state_info[state];

      for_const_contents (list<GFF>, info.gff, gff_tmpl)
	{
	  GFF gff (*gff_tmpl);

	  gff.seqname = seqname;
	  gff.start = subseq.start + 1;
	  gff.end = subseq.end();
	  gff.score = Nats2Bits (ll);

	  map<sstring,sstring> group_key_val;

	  if (pp_calc)
	    for (int s = 0; s < states(); ++s)
	      {
		sstring pp_val, pp_tag;
		pp_tag << ECFG_GFF_LogPostProb_tag << '(' << state_info[s].name << ')';
		pp_val << Nats2Bits (pp_calc->post_state_ll (s, subseq));
		group_key_val[pp_tag] = pp_val;
	      }

	  if (ins_calc)
	    for (int s = 0; s < states(); ++s)
	      {
		sstring ins_val, ins_tag;
		ins_tag << ECFG_GFF_LogInsideProb_tag << '(' << state_info[s].name << ')';
		ins_val << Nats2Bits (ins_calc->state_inside_ll (s, subseq));
		group_key_val[ins_tag] = ins_val;
	      }

	  gff.set_values (group_key_val);

	  gff_list.push_back (gff);
	}
    }
}

bool ECFG_scores::has_parametric_functions() const
{
  if (has_parametric_transitions)
    return true;

  for_const_contents (vector<ECFG_chain>, matrix_set.chain, chain)
    if (chain->is_parametric)
      return true;

  for_const_contents (vector<ECFG_state_info>, state_info, info)
    if (info->has_parametric_indels)
      return true;

  return false;
}

bool ECFG_scores::has_hidden_classes() const
{
  for_const_contents (vector<ECFG_chain>, matrix_set.chain, chain)
    if (chain->classes > 1)
      return true;

  return false;
}

bool ECFG_scores::has_GFF() const
{
  for_const_contents (vector<ECFG_state_info>, state_info, info)
    if (!info->gff.empty())
      return true;
  return false;
}

void ECFG_scores::eval_funcs()
{
  if (has_parametric_transitions)
    for (int s = Start; s < states(); ++s)
      for (int d = End; d < states(); ++d)
	if (s != End && d != Start)
	  {
	    const PFunc& pfunc = trans_funcs.transition (s, d);
	    transition (s, d) = pfunc.is_null() ? -InfinityScore : pfunc.eval_sc (pscores);
	  }

  for_contents (vector<ECFG_chain>, matrix_set.chain, chain)
    if (chain->is_parametric)
      {
	const int chain_states = chain->matrix->m();
	for (int s = 0; s < chain_states; ++s)
	  {
	    const PFunc& pi_pfunc = chain->matrix_funcs->pi[s];
	    chain->matrix->pi[s] = pi_pfunc.is_null() ? 0. : Score2Prob (pi_pfunc.eval_sc (pscores));

	    if (CTAGGING(-3,ECFG_SCORES))
	      {
		CL << "Initial probability for state (";
		ECFG_builder::print_state (CL, s, chain->word_len, alphabet, chain->class_labels);
		CL << ") = (";
		ECFG_builder::pfunc2stream (CL, pscores, pi_pfunc);
		CL << ") evaluates to " << chain->matrix->pi[s] << "\n";
	      }

	    chain->matrix->X[0] (s, s) = 0.;
	    for (int d = 0; d < chain_states; ++d)
	      if (s != d)
		{
		  const PFunc& rate_pfunc = chain->matrix_funcs->X[0] (s, d);
		  const double rate = rate_pfunc.is_null() ? 0. : Score2Prob (rate_pfunc.eval_sc (pscores));
		  chain->matrix->X[0] (s, d) = rate;
		  chain->matrix->X[0] (s, s) -= rate;

		  if (CTAGGING(-3,ECFG_SCORES))
		    {
		      CL << "Rate from (";
		      ECFG_builder::print_state (CL, s, chain->word_len, alphabet, chain->class_labels);
		      CL << ") to (";
		      ECFG_builder::print_state (CL, d, chain->word_len, alphabet, chain->class_labels);
		      CL << ") = (";
		      ECFG_builder::pfunc2stream (CL, pscores, rate_pfunc);
		      CL << ") evaluates to " << rate << "\n";
		    }
		}
	  }
	if (CTAGGING(-2,ECFG_SCORES))
	  {
	    CL << "Chain (terminal " << chain->state << ") evaluated from PFunc's:\n";
	    chain->matrix->write (CL);
	  }
	chain->matrix->update();
      }

  for_contents (vector<ECFG_state_info>, state_info, info)
    if (info->has_parametric_indels)
      {
	info->link_extend = Score2Prob (info->link_extend_func.eval_sc (pscores));
	info->link_end = Score2Prob (info->link_end_func.eval_sc (pscores));
	info->ins_rate = Score2Prob (info->ins_rate_func.eval_sc (pscores));
	info->del_rate = Score2Prob (info->del_rate_func.eval_sc (pscores));
	// too lazy to add log messages for these PFunc's; see above examples for (mutate...) and (initial...) -- IH 6/2/2006
      }

  if (CTAGGING(-3,ECFG_TRANS_SCORES))
    {
      CL << "ECFG_scores transition matrix:\n";
      show_transitions (CL);
    }

  if (CTAGGING(-1,ECFG_SCORES))
    {
      CL << "Evaluated ECFG_scores from PFunc's\n";
      pscores.show (CL);
      if (has_parametric_transitions)
	show_transitions (CL);
      for_const_contents (vector<ECFG_chain>, matrix_set.chain, chain)
	if (chain->is_parametric)
	  {
	    CL << "Chain (terminal " << chain->state << "):\n";
	    chain->matrix->write (CL);
	  }
      for_const_contents (vector<ECFG_state_info>, state_info, info)
	if (info->has_parametric_indels)
	  {
	    CL << "Gap model for state " << info->name << ":\n";
	    CL << '(' << EG_TRANSFORM_EXTEND_PROB << ' ' << info->link_extend
	       << ") (" << EG_TRANSFORM_END_PROB << ' ' << info->link_end
	       << ") (" << EG_TRANSFORM_INS_RATE << ' ' << info->ins_rate
	       << ") (" << EG_TRANSFORM_DEL_RATE << ' ' << info->del_rate
	       << ")\n";
	  }
    }
}

void ECFG_scores::remove_fake_bifurcation_transitions()
{
  for (int s = 0; s < states(); ++s)
    if (state_info[s].bifurc)
      {
	transition (s, state_info[s].ldest) = -InfinityScore;
	transition (s, state_info[s].rdest) = -InfinityScore;
      }
}

void ECFG_scores::add_fake_bifurcation_transitions()
{
  // first add fake transitions for all bifurcations
  for (int s = 0; s < states(); ++s)
    if (state_info[s].bifurc)
      {
	transition (s, state_info[s].ldest) = 0;
	transition (s, state_info[s].rdest) = 0;
      }

  // now figure out which non-emit states have a path to End
  // not sure if this algorithm accurately detects all such paths involving bifurcations.... hmmm...
  const vector<vector<int> > incoming = Transition_methods::incoming_states (*this);
  vector<int> is_empty (states(), 0);
  set<int> new_empties;
  for (int s = 0; s < states(); ++s)
    if (!state_info[s].bifurc && state_info[s].emit_size() == 0 && transition(s,End) > -InfinityScore)
      new_empties.insert (s);
  while (new_empties.size())
    {
      for_const_contents (set<int>, new_empties, s)
	is_empty[*s] = 1;
      set<int> next_new_empties;
      for_const_contents (set<int>, new_empties, dest)
	{
	  for_const_contents (vector<int>, incoming[*dest], src)
	    {
	      const ECFG_state_info& info = state_info[*src];
	      if (info.emit_size() == 0)
		{
		  if (info.bifurc)
		    {
		      if (is_empty[info.ldest] && is_empty[info.rdest])
			next_new_empties.insert (*src);
		    }
		  else
		    next_new_empties.insert (*src);
		}
	    }
	}
      new_empties.swap (next_new_empties);
    }

  // now remove all fake bifurcation transitions, *except* from A->B, where the bifurcation is A->BC (or A->CB) and C is empty.
  for (int s = 0; s < states(); ++s)
    if (state_info[s].bifurc)
      {
	if (!is_empty[state_info[s].rdest])
	  transition (s, state_info[s].ldest) = -InfinityScore;
	if (!is_empty[state_info[s].ldest])
	  transition (s, state_info[s].rdest) = -InfinityScore;
      }
}

void ECFG_scores::show (ostream& out) const
{
  for (int s = 0; s < states(); ++s)
    {
      out << "State " << s << ": ";
      state_info[s].show (out);
    }
  out << "Transition scores:\n";
  show_transitions (out);
}

ECFG_simulation::ECFG_simulation (const ECFG_scores& ecfg, const PHYLIP_tree& tree)
  : Stockade (0, 0),
    ecfg (ecfg),
    counts (ecfg)
{
  // sample parse tree
  vector<int> emission_state;
  vector<vector<int> > emission_id;
  vector<int> id_emission;
  list<int> seq;
  list<int>::iterator pos = seq.end();
  stack<list<int>::iterator> pos_stack;
  stack<int> state_stack;
  int state = Start;
  while (true)
    {
      // if bifurcation, fork
      if (state < 0 ? false : ecfg.state_info[state].bifurc)
	{
	  if (CTAGGING(0,ECFG_SIM))
	    CL << "Bifurcating: "
	       << ecfg.state_info[state].name
	       << " => "
	       << ecfg.state_info[ecfg.state_info[state].ldest].name
	       << ' '
	       << ecfg.state_info[ecfg.state_info[state].rdest].name
	       << '\n';

	  pos_stack.push (pos);
	  state_stack.push (ecfg.state_info[state].rdest);
	  state = ecfg.state_info[state].ldest;
	  continue;
	}

      // sample next state
      vector<Prob> dest_prob;
      for (int dest = 0; dest < ecfg.states(); ++dest)
	dest_prob.push_back (Score2Prob (ecfg.transition (state, dest)));
      dest_prob.push_back (Score2Prob (ecfg.transition (state, End)));

      if (CTAGGING(-1,ECFG_SIM))
	CL << "State "
	   << state
	   << ", dest_prob: ("
	   << dest_prob
	   << ")\n";

      int dest = Rnd::choose (dest_prob);
      if (dest == ecfg.states())
	dest = End;

      // increment transition count
      ++counts.transition (state, dest);

      // log
      if (CTAGGING(0,ECFG_SIM))
	CL << "Transforming: "
	   << (state == Start ? ECFG_default_start_nonterminal : ecfg.state_info[state].name.c_str())
	   << " => "
	   << (dest == End ? "End" : ecfg.state_info[dest].name.c_str())
	   << '\n';

      state = dest;

      // if end state, pop next state off the stack, or stop
      if (state == End)
	{
	  if (state_stack.empty())
	    break;
	  pos = pos_stack.top();
	  state = state_stack.top();
	  pos_stack.pop();
	  state_stack.pop();

	  if (CTAGGING(0,ECFG_SIM))
	    CL << "Popping state "
	       << ecfg.state_info[state].name.c_str()
	       << " from the stack\n";

	  continue;
	}

      // if emit state, create the emission
      if (ecfg.state_info[state].emit_size())
	{
	  const ECFG_state_info& info = ecfg.state_info[state];
	  if (info.l_context || info.r_context)
	    THROWEXPR ("Sorry, something you need is unimplemented: you can't sample from emit states with context");
	  if (info.indels)
	    THROWEXPR ("Sorry, something you need is unimplemented: you can't sample from states with indel models");

	  // now we have to figure out positions from mul vector. ugh
	  vector<int> sym_pos;
	  for (int i = 0; i < (int) info.mul.size(); ++i)
	    {
	      int p = 0;
	      for (int m = info.mul[i]; m > 1; m = m / ecfg.alphabet.size())
		++p;
	      sym_pos.push_back (p);
	    }

	  // create IDs for the emitted columns
	  const int emission = emission_state.size();
	  vector<int> col_id (sym_pos.size());
	  for (int c = 0; c < (int) sym_pos.size(); ++c)
	    {
	      const int e = id_emission.size();
	      col_id[sym_pos[c]] = e;
	      id_emission.push_back (emission);
	    }
	  emission_state.push_back (state);
	  emission_id.push_back (col_id);

	  // insert the emitted column IDs into seq
	  seq.insert (pos, col_id.begin(), col_id.end());

	  // move the insertion iterator
	  for (int i = 0; i < info.r_emit; ++i)
	    --pos;
	}
    }

  // build map of column ID to seq positions
  vector<int> id2col (id_emission.size());
  int n_col = 0;
  for_const_contents (list<int>, seq, col_id)
    id2col[*col_id] = n_col++;

  // build map of emission ID to seq positions
  vector<vector<int> > emission_col (emission_id);
  for (int e = 0; e < (int) emission_id.size(); ++e)
    for (int c = 0; c < (int) emission_id[e].size(); ++c)
      emission_col[e][c] = id2col[emission_id[e][c]];

  // figure out which rows we're going to show (the ones with names!)
  map<int,int> node2row;
  for (int n = 0, r = 0; n < tree.nodes(); ++n)
    if (tree.node_name[n].size())
      node2row[n] = r++;

  // create the Stockade
  Stockade stockade (node2row.size(), seq.size());
  for (int n = 0; n < tree.nodes(); ++n)
    if (node2row.find(n) != node2row.end())
      {
	const int r = node2row[n];
	stockade.align.row_name[r]
	  = stockade.np[r].name
	  = tree.node_name[n];

	stockade.np[r].seq = Biosequence (stockade.align.columns());
	stockade.np[r].dsq = Digitized_biosequence (stockade.align.columns());
	stockade.np[r].prof_w = Weight_profile (stockade.align.columns());
	stockade.np[r].prof_sc = Score_profile (stockade.align.columns());

	for (int c = 0; c < stockade.align.columns(); ++c)
	  stockade.align.path.row(r)[c] = true;
      }

  // cache pointers to substitution rate matrices (for each branch) & initial state distributions (for root node)
  vector<vector<array2d<double>*> > rate_matrix_cache (ecfg.matrix_set.chain.size(),
						       vector<array2d<double>*> (tree.nodes(),
										 (array2d<double>*) 0));  // indexed by [chain][node]
  vector<vector<Substitution_matrix_factory*> > submat_cache (ecfg.matrix_set.chain.size(),
							      vector<Substitution_matrix_factory*> (tree.nodes(),
												    (Substitution_matrix_factory*) 0));  // indexed by [chain][node]
  vector<vector<double>*> root_pi_cache (ecfg.matrix_set.chain.size(), (vector<double>*) 0);  // indexed by chain

  // also EM_matrix_base::Update_statistics for branches & root nodes
  vector<vector<Update_statistics*> > branch_stats (ecfg.matrix_set.chain.size(),
						    vector<Update_statistics*> (tree.nodes(),
										(Update_statistics*) 0));  // indexed by [chain][node]
  vector<Update_statistics*> root_stats (ecfg.matrix_set.chain.size(), (Update_statistics*) 0);  // indexed by chain

  // now populate these root/branch lookup structures...
  for (int c = 0; c < (int) ecfg.matrix_set.chain.size(); ++c)
    {
      const ECFG_chain& chain = ecfg.matrix_set.chain[c];
      if (chain.matrix)  // hybrid chain?
	{
	  // not a hybrid chain...
	  EM_matrix_base& em_mat = *chain.matrix;
	  Update_statistics* stats = &counts.stats[c];
	  for_rooted_branches_pre (tree, b)
	    {
	      const int n = (*b).second;
	      rate_matrix_cache[c][n] = &em_mat.X[0];
	      submat_cache[c][n] = &em_mat;
	      branch_stats[c][n] = stats;
	    }
	  root_pi_cache[c] = &em_mat.pi;
	  root_stats[c] = stats;

	  // print log message
	  if (CTAGGING(0,ECFG_SIM))
	    {
	      CL << "Chain " << c << " is not a hybrid chain...\n";
	      CL << "Rate matrix (all branches):\n" << em_mat.X[0];
	      CL << "Initial state distribution: (" << *root_pi_cache[c] << ")\n";
	    }
	}
      else
	{
	  // hybrid chain
	  // can only do gs_val=node_name at present
	  if (chain.gs_tag != ECFG_IMPLICIT_GS_TAG_NODENAME)
	    THROWEXPR ("Can only currently simulate from hybrid chains that use the implicit "
		       << Stockholm_sequence_annotation << " tag, '" << ECFG_IMPLICIT_GS_TAG_NODENAME
		       << "', and so are indexed by node-name. Sorry if this throws a spanner in your works.");

	  // default chain #=GS value is the first one in the gs_values[] list
	  const int default_chain_index = ((map<sstring,int>&) chain.gs_tag_value_chain_index)[chain.gs_values[0]];  // cast away const

	  // print log message
	  CTAG(0,ECFG_SIM) << "Chain " << c << " is a hybrid chain...\n";

	  for_rooted_nodes_pre (tree, b)
	    {
	      const int n = (*b).second;
	      
	      // set gs_val to the node name
	      const sstring& gs_val = tree.node_name[n];

	      // select the appropriate chain using gs_val
	      int chain_index = -1;
	      if (chain.gs_tag_value_chain_index.find (gs_val) != chain.gs_tag_value_chain_index.end())
		chain_index = ((map<sstring,int>&) chain.gs_tag_value_chain_index)[gs_val];  // cast away const
	      else if (chain.gs_values.size())  // use default label?
		{
		  chain_index = default_chain_index;
		  CTAG(4,ECFG_SIM) << "No component chain with "
				   << Stockholm_sequence_annotation
				   << " label '" << gs_val
				   << "' in hybrid chain (" << chain.state
				   << "); using default label '"
				   << chain.gs_values[0] << "' instead\n";
		}
	      else
		THROWEXPR ("No component chain with "
			   << Stockholm_sequence_annotation
			   << " label '" << gs_val
			   << "' in hybrid chain (" << chain.state
			   << "), and no default label");
	      
	      EM_matrix_base& em_mat = *ecfg.matrix_set.chain[chain_index].matrix;
	      rate_matrix_cache[c][n] = &em_mat.X[0];
	      submat_cache[c][n] = &em_mat;

	      Update_statistics* stats = &counts.stats[chain_index];
	      branch_stats[c][n] = stats;

	      if (n == tree.root)
		{
		  root_pi_cache[c] = &em_mat.pi;
		  root_stats[c] = stats;
		}

	      // print log message
	      CTAG(0,ECFG_SIM) << "Rate matrix (branch " << n << "):\n" << rate_matrix_cache[c][n];
	    }

	  // check that root_pi_cache was set
	  if (root_pi_cache[c] == 0)
	    THROWEXPR ("Can't simulate, since hybrid chain has no root matrix from which I can garner an initial state distribution");

	  // print log message
	  CTAG(0,ECFG_SIM) << "Initial state distribution: (" << *root_pi_cache[c] << ")\n";
	}
    }

  // sample alignment columns
  for (int e = 0; e < (int) emission_id.size(); ++e)
    {
      const ECFG_state_info& info = ecfg.state_info[emission_state[e]];
      vector<double>& root_pi (*root_pi_cache[info.matrix]);

      // add annotations
      for_const_contents (ECFG_state_info::ECFG_state_annotation, info.annot, tag_val)
	{
	  const sstring& tag = tag_val->first;
	  const sstring& val = tag_val->second;
	  if (stockade.align.gc_annot.find (tag) == stockade.align.gc_annot.end())
	    stockade.align.set_gc_annot (tag, sstring (stockade.align.columns(), ECFG_annotation_wildcard));
	  if (val.size() != emission_col[e].size())
	    THROWEXPR ("Ulp. Wrong number of characters in ECFG annotation field");
	  for (int c = 0; c < (int) emission_col[e].size(); ++c)
	    stockade.align.gc_annot[tag][emission_col[e][c]] = val[c];
	}

      // simulate the Markov chain on the tree
      vector<int> node_state (tree.nodes());
      node_state[tree.root] = Rnd::choose (root_pi);
      ++root_stats[info.matrix]->s[node_state[tree.root]];  // increment start count
      for_rooted_branches_pre (tree, b)
	{
	  const int
	    p = (*b).first,
	    c = (*b).second;
	  const double
	    t = (*b).length;
	  const array2d<double>& rate_matrix = *rate_matrix_cache[info.matrix][c];
	  Update_statistics& stats = *branch_stats[info.matrix][c];
	  int state = node_state[p];
	  double tsim = 0.;
	  vector<double> rate_matrix_row (rate_matrix.xsize());
	  while (true)
	    {
	      double r = 0.;
	      for (int j = 0; j < (int) root_pi.size(); ++j)
		r += (rate_matrix_row[j] = (j==state
					    ? 0.
					    : rate_matrix (state, j)));

	      double wait_time = -Math_fn::math_log (Rnd::prob()) / r;

	      if (tsim + wait_time > t)
		{
		  stats.w[state] += t - tsim;  // increment wait time
		  break;
		}

	      tsim += wait_time;
	      stats.w[state] += wait_time;  // increment wait time

	      const int next_state = Rnd::choose (rate_matrix_row);
	      ++stats.u(state,next_state);  // increment transition usage
	      state = next_state;
	    }
	  node_state[c] = state;
	  if (CTAGGING(1,ECFG_SIM))
	    CL << "Emission " << e
	       << ", columns (" << emission_col[e]
	       << "), node " << c
	       << ", parent " << p
	       << ": parent state " << node_state[p]
	       << ", child state " << node_state[c]
	       << '\n';
	}

      // log
      CTAG(3,ECFG_SIM) << "Emission " << e
		       << ", columns (" << emission_col[e]
		       << "): node_state (" << node_state
		       << ")\n";

      // populate the Named_profile's
      for (int n = 0; n < tree.nodes(); ++n)
	if (node2row.find(n) != node2row.end())
	  {
	    const int r = node2row[n];
	    for (int c = 0, mul = 1; c < (int) emission_col[e].size(); ++c)
	      {
		Named_profile& np = stockade.np[r];
		const int ecol = emission_col[e][c];

		int esym = (node_state[n] / mul) % ecfg.alphabet.size();
		if (info.comp[c])
		  esym = ecfg.alphabet.complement (esym);

		const int echar = ecfg.alphabet.int2char (esym);
		Symbol_score_map essm;

		np.seq[ecol] = echar;
		np.dsq[ecol] = esym;
		np.prof_w[ecol][esym] = (Prob) 1.;
		np.prof_sc[ecol][esym] = (Score) 0;

		mul = mul * ecfg.alphabet.size();
	      }
	  }
    }

  // add tree to Stockade
  sstring tree_string;
  tree.write (tree_string, 0);
  tree_string.chomp();
  stockade.align.add_gf_annot (sstring (Stockholm_New_Hampshire_tag), tree_string);

  // assign Stockade
  ((Stockade&) *this) = stockade;
}

void ECFG_simulation::add_counts_to_stockade()
{
  sstring counts_text;
  ECFG_builder::chain_counts2stream (counts_text, ecfg.alphabet, ecfg, counts);
  const vector<sstring> counts_lines = counts_text.split ("\n");
  for_const_contents (vector<sstring>, counts_lines, cl)
    align.add_gf_annot (sstring (EG_CHAIN_COUNTS), *cl);
}

ECFG_counts::ECFG_counts (const ECFG_scores& ecfg) :
  ECFG<Prob> (ecfg, 0.),
  ecfg (&ecfg),
  stats (ecfg.matrix_set.chain.size(), Update_statistics (0)),
  filled_down (ecfg.matrix_set.chain.size(), false),

  ins_count (ecfg.states(), 0.),
  del_count (ecfg.states(), 0.),
  ins_wait (ecfg.states(), 0.),
  del_wait (ecfg.states(), 0.),
  link_extend_count (ecfg.states(), 0.),
  link_end_count (ecfg.states(), 0.),

  var_counts (ecfg.pscores)
{
  for (int i = 0; i < (int) stats.size(); ++i)
    if (ecfg.matrix_set.chain[i].type != Hybrid)
      stats[i] = Update_statistics (ecfg.matrix_set.chain[i].matrix->m());
}

void ECFG_counts::clear (double pseud_init, double pseud_mutate, double pseud_wait)
{
  for (int i = 0; i < (int) stats.size(); ++i)
    {
      if (ecfg->matrix_set.chain[i].type == Parametric)  // don't use pseudocounts on parametric chains
	stats[i].clear (0., 0., 0.);
      else
	stats[i].clear (pseud_init, pseud_mutate, pseud_wait);
    }
  for (int src = Start; src < states(); ++src)
    {
      transition (src, End) = 0.;
      for (int dest = 0; dest < states(); ++dest)
	transition (src, dest) = 0.;
    }
  for (int s = 0; s < states(); ++s)
    {
      ins_count[s] = del_count[s] = ins_wait[s] = del_wait[s] = 0.;
      link_extend_count[s] = link_end_count[s] = 0.;
    }
}

void ECFG_counts::update_ecfg (ECFG_scores& ecfg)
{
  // if nothing to update, bail
  const bool update_params = ecfg.has_parametric_functions();
  if (!update_params && !ecfg.update_rates && !ecfg.update_rules)
    {
      CLOGERR << "Warning: ECFG is not updatable\n";
      return;
    }

  // update the various bits
  if (ecfg.update_rates)
    update_ecfg_rates (ecfg);

  if (ecfg.update_rules)
    update_ecfg_rules (ecfg);

  if (update_params)
    update_ecfg_params (ecfg);
}

void ECFG_counts::update_ecfg_rates (ECFG_scores& ecfg)
{
  // update substitution rates
  for (int i = 0; i < (int) stats.size(); ++i)
    {
      // skip if this is a lineage-dependent model
      if (ecfg.matrix_set.chain[i].type == Hybrid)
	continue;

      // skip if fill_down() not called
      if (!filled_down[i])
	continue;

      // update EM_matrix_base
      EM_matrix_base& em_matrix (*ecfg.matrix_set.chain[i].matrix);
      stats[i].transform (em_matrix, true);
      if (CTAGGING(2,ECFG_STATS))
	CL << "Update statistics for matrix #" << i << ":\n" << stats[i];
      em_matrix.quick_M (stats[i], true, true);
    }

  // update pseudo-indel rates
  for (int src = Start; src < ecfg.states(); ++src)
    {
      ECFG_state_info* info = src >= 0 ? &ecfg.state_info[src] : 0;
      if (info)
	if (info->indels)
	  {
	    if (ins_wait[src] > 0.)
	      info->ins_rate = ins_count[src] / ins_wait[src];

	    if (del_wait[src] > 0.)
	      info->del_rate = del_count[src] / del_wait[src];

	    const double link_norm = link_extend_count[src] + link_end_count[src];
	    if (link_norm)
	      {
		info->link_extend = link_extend_count[src] / link_norm;
		info->link_end = link_end_count[src] / link_norm;
	      }
	  }
    }
}

void ECFG_counts::update_ecfg_rules (ECFG_scores& ecfg)
{
  // log counts
  if (CTAGGING(2,ECFG_STATS))
    {
      CL << "ECFG transition counts:\n";
      show_transitions (CL);
      for (int s = 0; s < ecfg.states(); ++s)
	if (ecfg.state_info[s].indels)
	  CL << "State #" << s << " indel counts: #extend=" << link_extend_count[s] << ", #end=" << link_end_count[s]
	     << ", #ins=" << ins_count[s] << ", inswait=" << ins_wait[s]
	     << ", #del=" << del_count[s] << ", delwait=" << del_wait[s]
	     << "\n";
    }

  // update rule probs
  for (int src = Start; src < ecfg.states(); ++src)
    {
      ECFG_state_info* info = src >= 0 ? &ecfg.state_info[src] : 0;
      if (info ? info->bifurc : false)
	continue;
      double norm = transition (src, End);
      for (int dest = 0; dest < ecfg.states(); ++dest)
	norm += transition (src, dest);
      if (norm > 0)
	{
	  ecfg.transition (src, End) = Prob2Score (transition (src, End) / norm);
	  for (int dest = 0; dest < ecfg.states(); ++dest)
	    ecfg.transition (src, dest) = Prob2Score (transition (src, dest) / norm);
	}
    }
}

void ECFG_counts::update_ecfg_params (ECFG_scores& ecfg)
{
  if (ecfg.pcounts.groups())
    var_counts = ecfg.pcounts;
  else
    var_counts.clear();
  inc_var_counts (var_counts, ecfg);

  if (CTAGGING(3,ECFG_COUNTS))
    {
      CL << "Converted ECFG_counts to PCounts:\n";
      var_counts.show (CL);
      if (ecfg.pcounts.groups())
	{
	  CL << "Pseudocounts:\n";
	  ecfg.pcounts.show (CL);
	}
    }

  for_const_contents (set<int>, ecfg.mutable_pgroups, g)
    var_counts.optimise_group (*g, ecfg.pscores);
}

void ECFG_counts::show (const ECFG_scores& ecfg, ostream& out) const
{
  for (int c = 0; c < (int) stats.size(); ++c)
    {
      out << "Chain " << c << "\n";
      out << stats[c];
    }
  for (int s = 0; s < ecfg.states(); ++s)
    if (ecfg.state_info[s].has_parametric_indels)
      {
	out << "Gap model for state " << s << ":\n";
	out << "ins_count = " << ins_count[s] << "\n";
	out << "ins_wait = " << ins_wait[s] << "\n";
	out << "del_count = " << del_count[s] << "\n";
	out << "del_wait = " << del_wait[s] << "\n";
	out << "link_extend_count = " << link_extend_count[s] << "\n";
	out << "link_end_count = " << link_end_count[s] << "\n";
      }
  if (ecfg.has_parametric_functions())
    {
      out << "Parameter counts:\n";
      var_counts.show (out);
    }
  out << "Transition counts:\n";
  show_transitions (out);
}

void ECFG_counts::inc_var_counts (PCounts&           var_counts,
				  const ECFG_scores& ecfg,
				  const Prob         weight)
{
  if (ecfg.has_parametric_transitions)
    for (int s = Start; s < states(); ++s)
      for (int d = End; d < states(); ++d)
	if (s != End && d != Start)
	  {
	    const PFunc& pfunc = ecfg.trans_funcs.transition (s, d);
	    if (!pfunc.is_null())
	      pfunc.inc_var_counts (var_counts, ecfg.pscores, weight * transition (s, d));
	  }

  for (int c = 0; c < (int) stats.size(); ++c)
    {
      const ECFG_chain& chain = ecfg.matrix_set.chain[c];
      if (chain.type == Hybrid)  // skip if this chain is a lineage-dependent model
	continue;

      if (chain.is_parametric)
	{
	  Update_statistics& chain_stats = stats[c];
	  const int chain_states = chain.matrix->m();
	  for (int s = 0; s < chain_states; ++s)
	    {
	      const PFunc& pi_pfunc = chain.matrix_funcs->pi[s];
	      if (!pi_pfunc.is_null())
		pi_pfunc.inc_var_counts (var_counts, ecfg.pscores, weight * chain_stats.s[s]);

	      for (int d = 0; d < chain_states; ++d)
		{
		  const PFunc& rate_pfunc = chain.matrix_funcs->X[0] (s, d);
		  if (!rate_pfunc.is_null())
		    {
		      const double u = (s == d)
			? (chain_stats.w[s] * Score2Prob (rate_pfunc.eval_sc (ecfg.pscores)))
			: chain_stats.u(s,d);
		      rate_pfunc.inc_var_counts (var_counts, ecfg.pscores, weight * u, weight * chain_stats.w[s]);
		    }
		}
	    }
	}
    }

  for (int s = 0; s < states(); ++s)
    {
      const ECFG_state_info& info = ecfg.state_info[s];
      if (info.has_parametric_indels)
	{
	  info.link_extend_func.inc_var_counts (var_counts, ecfg.pscores, weight * link_extend_count[s]);
	  info.link_end_func.inc_var_counts (var_counts, ecfg.pscores, weight * link_end_count[s]);
	  info.ins_rate_func.inc_var_counts (var_counts, ecfg.pscores, weight * ins_count[s], weight * ins_wait[s]);
	  info.del_rate_func.inc_var_counts (var_counts, ecfg.pscores, weight * del_count[s], weight * del_wait[s]);
	}
    }
}

void ECFG_state_info::show (ostream& out) const
{
  out << "name=" << name << " bifurc=" << bifurc << " ldest=" << ldest << " rdest=" << rdest << " matrix=" << matrix;
  out << " l_context=" << l_context << " l_emit=" << l_emit << " r_emit=" << r_emit << " r_context=" << r_context;

  out << " mul=(" << mul << ')';

  out << " comp=(" << comp << ')';

  out << '\n';
}
