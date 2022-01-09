/*
 * Copyright (C) 2022 Brodie Gaslam
 *
 * This file is part of "fansi - ANSI Control Sequence Aware String Functions"
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Go to <https://www.r-project.org/Licenses> for a copies of the licenses.
 */

#include "fansi.h"

/*
 * Trim leading or trailing whitespaces intermixed with control sequences.
 *
 * @param which 0 = both, 1 = left, 2 = right
 */

SEXP FANSI_trimws(
  SEXP x, SEXP which, SEXP warn, SEXP term_cap, SEXP ctl, SEXP norm
) {
  if(TYPEOF(x) != STRSXP)
    error("Argument `x` should be a character vector.");     // nocov
  if(TYPEOF(ctl) != INTSXP)
    error("Internal Error: `ctl` should integer.");          // nocov
  if(TYPEOF(which) != INTSXP || XLENGTH(which) != 1)
    error("Internal Error: `which` should scalar integer."); // nocov
  if(TYPEOF(norm) != LGLSXP || XLENGTH(norm) != 1)
    error("Internal Error: `norm` should scalar logical.");  // nocov

  const char * arg = "x";
  R_xlen_t i, len = XLENGTH(x);
  SEXP res_fin = x;
  int which_i = asInteger(which);
  if(which_i < 0 || which_i > 2)
    error("Internal Error: `which` must be between 0 and 2."); // nocov

  int norm_i = asLogical(norm);

  int prt = 0;
  PROTECT_INDEX ipx;
  // reserve spot if we need to alloc later
  PROTECT_WITH_INDEX(res_fin, &ipx); ++prt;

  struct FANSI_state state, state_lead, state_trail, state_last;
  struct FANSI_buff buff;
  FANSI_INIT_BUFF(&buff);
  SEXP R_false = PROTECT(ScalarLogical(0)); ++prt;
  SEXP R_zero = PROTECT(ScalarInteger(0)); ++prt;
  for(i = 0; i < len; ++i) {
    if(!i) {
      SEXP allowNA, keepNA, type;
      type = R_zero;
      allowNA = keepNA = R_false;
      state = FANSI_state_init_full(
        x, warn, term_cap, allowNA, keepNA, type, ctl, (R_xlen_t) 0
      );
    } else FANSI_state_reinit(&state, x, i);
    state_lead = state_trail = state_last = state;

    SEXP x_chr = STRING_ELT(x, i);
    if(x_chr == NA_STRING) continue;
    FANSI_interrupt(i);

    // Two (really three) pass process: find begin and end points of string to
    // keep, compute required size for final string (due to normalize and other
    // factors, can't really know just based on input), and finally write.  The
    // last two are part of the standard two pass measure/write framework used
    // in fansi.
    int string_start = 0;
    if(which_i == 0 || which_i == 1) {
      while(1) {
        switch(state.string[state.pos.x]) {
          case ' ':
          case '\n':
          case '\r':
          case '\t':
            ++state.pos.x;
            continue;
          default:
            if(IS_PRINT(state.string[state.pos.x])) {
              goto ENDLEAD;
            } else {
              struct FANSI_state state_tmp = state;
              FANSI_read_next(&state_tmp, i, arg);
              state.status |= state_tmp.status & STAT_WARNED;
              if(state_tmp.status & CTL_MASK) {
                state = state_tmp;
                break;  // break out of switch, NOT out of while
              }
              else goto ENDLEAD;
            }
      } }
      ENDLEAD:
      state_lead = state;
      string_start = state_lead.pos.x;
    }
    // Find first space that has no subsequent non-spaces
    int string_end = -1; // -1 dissambiguates something with nothing but spaces
    if(which_i == 0 || which_i == 2) {
      while(state.string[state.pos.x]) {
        switch(state.string[state.pos.x]) {
          case ' ':
          case '\n':
          case '\r':
          case '\t':
            if(string_end < 0) {
              string_end = state.pos.x;
              state_trail = state;
            }
            ++state.pos.x;
            continue;
          default:
            if(IS_PRINT(state.string[state.pos.x])) {
              string_end = -1;
              ++state.pos.x;
            } else {
              struct FANSI_state state_tmp = state;
              FANSI_read_next(&state_tmp, i, arg);
              state.status |= state_tmp.status & STAT_WARNED;
              if(state_tmp.status & CTL_MASK) {
                state = state_tmp;
                continue;
              } else {
                string_end = -1;
                ++state.pos.x;
            } }
      } }
      state_last = state;
    }
    if(string_end < 0) {
      string_end = LENGTH(x_chr);
      state_trail = state_last;
    }
    // Do we need to write the string?
    if(string_start || string_end != LENGTH(x_chr)) {
      // We need a new vector since we have at least one change
      if(res_fin == x) REPROTECT(res_fin = duplicate(x), ipx);
      const char * err_msg = "Trimming whitespace";

      // Two pass measure/write (see write.c)
      for(int k = 0; k < 2; ++k) {
        if(!k) FANSI_reset_buff(&buff);
        else   FANSI_size_buff(&buff);
        FANSI_state_reinit(&state, x, i);

        // Any leading SGR
        if(string_start) {
          FANSI_W_sgr(&buff, state_lead.fmt.sgr, norm_i, 1, i);
          FANSI_W_url(&buff, state_lead.fmt.url, i);
        }
        // Body of string
        FANSI_W_normalize_or_copy(
          &buff, state_lead, norm_i, string_end, i, err_msg, "x"
        );
        // Trailing state
        if(string_end)
          FANSI_W_bridge(&buff, state_trail, state_last, norm_i, i, err_msg);
      }
      // We assume UTF-8 can only show up in the body of the string
      SEXP chr_sexp = PROTECT(FANSI_mkChar(buff, getCharCE(x_chr), i));
      SET_STRING_ELT(res_fin, i, chr_sexp);
      UNPROTECT(1);
    }
  }
  FANSI_release_buff(&buff, 1);
  UNPROTECT(prt);
  return res_fin;
}
