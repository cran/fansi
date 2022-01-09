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

SEXP FANSI_unhandled_esc(SEXP x, SEXP term_cap) {
  if(TYPEOF(x) != STRSXP)
    error("Argument `x` must be a character vector.");  // nocov
  if(TYPEOF(term_cap) != INTSXP)
    error("Argument `term_cap` must be an integer vector.");  // nocov

  R_xlen_t x_len = XLENGTH(x);
  if(x_len >= FANSI_lim.lim_int.max)
    // nocov start
    error(
      "This function does not support vectors of length INT_MAX or longer."
    );
    // nocov end

  SEXP R_true = PROTECT(ScalarLogical(1));
  SEXP R_one = PROTECT(ScalarInteger(1));
  SEXP no_warn = PROTECT(ScalarInteger(0));
  SEXP ctl_all = PROTECT(ScalarInteger(0));
  SEXP res, res_start, allowNA, keepNA, width;
  res = res_start = R_NilValue;
  allowNA = keepNA = R_true;
  width = R_one;

  // reserve spot if we need to alloc later

  PROTECT_INDEX ipx;
  PROTECT_WITH_INDEX(res, &ipx);

  int any_errors = 0;
  int err_count = 0;
  int break_early = 0;
  struct FANSI_state state;
  const char * arg = "x";

  for(R_xlen_t i = 0; i < x_len; ++i) {
    FANSI_interrupt(i);
    SEXP chrsxp = STRING_ELT(x, i);
    if(!i) {
      state = FANSI_state_init_full(
        x, no_warn, term_cap, allowNA, keepNA, width, ctl_all, i
      );
      // Read one escape at a time
      state.settings |= SET_ESCONE;
    } else FANSI_state_reinit(&state, x, i);

    if(chrsxp != NA_STRING && LENGTH(chrsxp)) {
      int has_errors = 0;
      int ctl_bytes_all = 0;

      while(state.string[state.pos.x]) {
        // Since we don't care about width, etc, we only use the state objects
        // to parse the ESC sequences and UTF8 characters.

        int esc_start = state.pos.w + ctl_bytes_all;
        int esc_start_byte = state.pos.x;
        int ctl_bytes = 0;
        FANSI_read_next(&state, i, arg);
        if(state.status & CTL_MASK) {
          ctl_bytes = state.pos.x - esc_start_byte;
          ctl_bytes_all += ctl_bytes;
        }
        if(FANSI_GET_ERR(state.status)) {
          if(err_count == FANSI_lim.lim_int.max) {
            warning(
              "%s%s",
              "There are more than INT_MAX unhandled sequences, returning ",
              "first INT_MAX errors."
            );
            break_early = 1;
            break;
          }
          if(!has_errors) has_errors = 1;

          SEXP err_vals = PROTECT(allocVector(INTSXP, 7));
          INTEGER(err_vals)[0] = i + 1;
          INTEGER(err_vals)[1] = esc_start + 1;
          INTEGER(err_vals)[2] = state.pos.w + ctl_bytes_all;
          INTEGER(err_vals)[3] = FANSI_GET_ERR(state.status);
          INTEGER(err_vals)[4] = 0;
          // need actual bytes so we can substring the problematic sequence, so
          // we don't use 1 based indexing like with the earlier values
          INTEGER(err_vals)[5] = esc_start_byte;
          INTEGER(err_vals)[6] = state.pos.x - 1;
          SEXP err_vals_list = PROTECT(list1(err_vals));

          if(!any_errors) {
            any_errors = 1;
            REPROTECT(err_vals_list, ipx);
            res = res_start = err_vals_list;
          } else {
            SETCDR(res, err_vals_list);
            res = CDR(res);
          }
          ++err_count;
          UNPROTECT(2);
        }
      }
      if(break_early) break;
    }
  }
  // Convert result to a list that we could easily turn into a DFs

  SEXP res_fin = PROTECT(allocVector(VECSXP, 6));
  SEXP res_idx = PROTECT(allocVector(INTSXP, err_count));
  SEXP res_esc_start = PROTECT(allocVector(INTSXP, err_count));
  SEXP res_esc_end = PROTECT(allocVector(INTSXP, err_count));
  SEXP res_err_code = PROTECT(allocVector(INTSXP, err_count));
  SEXP res_translated = PROTECT(allocVector(LGLSXP, err_count));
  SEXP res_string = PROTECT(allocVector(STRSXP, err_count));

  res = res_start;

  for(int i = 0; i < err_count; ++i) {
    FANSI_interrupt((R_xlen_t) i);
    if(res == R_NilValue)
      // nocov start
      error(
        "%s%s",
        "Internal Error: mismatch between list and err count; "
        "contact maintainer."
      );
      // nocov end
    INTEGER(res_idx)[i] = INTEGER(CAR(res))[0];
    INTEGER(res_esc_start)[i] = INTEGER(CAR(res))[1];
    INTEGER(res_esc_end)[i] = INTEGER(CAR(res))[2];
    INTEGER(res_err_code)[i] = INTEGER(CAR(res))[3];
    LOGICAL(res_translated)[i] = INTEGER(CAR(res))[4];

    int byte_start = INTEGER(CAR(res))[5];
    int byte_end = INTEGER(CAR(res))[6];

    SEXP cur_chrsxp = STRING_ELT(x, INTEGER(res_idx)[i] - 1);

    if(
      byte_start < 0 || byte_end < 0 || byte_start >= LENGTH(cur_chrsxp) ||
      byte_end >= LENGTH(cur_chrsxp)
    )
      // nocov start
      error(
        "%s%s",
        "Internal Error: illegal byte offsets for extracting unhandled seq; ",
        "contact maintainer."
      );
      // nocov end

    SET_STRING_ELT(res_string, i,
      mkCharLenCE(
        CHAR(cur_chrsxp) + byte_start, byte_end - byte_start + 1,
        getCharCE(cur_chrsxp)
    ) );
    res = CDR(res);
  }
  SET_VECTOR_ELT(res_fin, 0, res_idx);
  SET_VECTOR_ELT(res_fin, 1, res_esc_start);
  SET_VECTOR_ELT(res_fin, 2, res_esc_end);
  SET_VECTOR_ELT(res_fin, 3, res_err_code);
  SET_VECTOR_ELT(res_fin, 4, res_translated);
  SET_VECTOR_ELT(res_fin, 5, res_string);
  UNPROTECT(12);
  return res_fin;
}
