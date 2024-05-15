set ts=2 sts=2 sw=2 expandtab " tabs

"""
" indentation rules
"  N-s : don't indent after C++ namespace
"  +s  : use a single indent after an incomplete line
"  (2s : use a double indent after any unclosed paren
"  Ws  : use a single indent after '($' specifically
"  ms  : put the ) at the same indent as (
"  t0  : don't indent after C++ template<...>
"  g0  : don't indent public / private / protected
"  c0  : don't indent after /*
"
"
"  use `:help cinoptions-values` for more info
set cino=N-s,+s,(2s,Ws,ms,t0,g0,c0
