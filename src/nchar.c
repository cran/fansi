/*
 * Copyright (C) 2018  Brodie Gaslam
 *
 *  This file is part of "fansi - ANSI Control Sequence Aware String Functions"
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Go to <https://www.r-project.org/Licenses/GPL-2> for a copy of the license.
 */

#include "fansi.h"

SEXP FANSI_nzchar(SEXP x, SEXP keepNA, SEXP warn, SEXP term_cap) {
  if(
    TYPEOF(x) != STRSXP ||
    TYPEOF(keepNA) != LGLSXP ||
    TYPEOF(warn) != LGLSXP ||
    TYPEOF(term_cap) != INTSXP
  )
    error("Internal error: input type error; contact maintainer"); // nocov

  int keepNA_int = asInteger(keepNA);
  int warn_int = asInteger(warn);
  int warned = 0;

  R_xlen_t x_len = XLENGTH(x);

  SEXP res = PROTECT(allocVector(LGLSXP, x_len));

  for(R_len_t i = 0; i < x_len; ++i) {
    FANSI_interrupt(i);
    SEXP string_elt = STRING_ELT(x, i);
    FANSI_check_enc(string_elt, i);

    if(string_elt == R_NaString) {
      if(keepNA_int == 1) {
        LOGICAL(res)[i] = NA_LOGICAL;
      } else LOGICAL(res)[i] = 1;
    } else {
      // Don't bother converting to UTF8

      const char * string = CHAR(string_elt);

      while((*string > 0 && *string < 32) || *string == 127) {
        struct FANSI_csi_pos pos = FANSI_find_esc(string, FANSI_STRIP_ALL);
        if(warn_int && !warned && (!pos.valid || (pos.what & (1 << 4)))) {
          warned = 1;
          warning(
            "Encountered %s ESC sequence at index [%.0f], %s%s",
            !pos.valid ? "invalid" : "possibly incorrectly handled",
            (double) i + 1,
            "see `?unhandled_ctl`; you can use `warn=FALSE` to turn ",
            "off these warnings."
          );
        }
        string += pos.len;
      }
      LOGICAL(res)[i] = *string != 0;
  } }
  UNPROTECT(1);
  return res;
}