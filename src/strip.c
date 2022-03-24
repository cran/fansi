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

static void FANSI_check_chr_size(char * start, char * end, R_xlen_t i) {
  if(end - start > FANSI_lim.lim_int.max) {
    // Can't get to this point with a string that violates, AFAICT
    // nocov start
    error(
      "Internal Error: %s at index [%jd] (3).",
      "attempting to write string longer than INT_MAX",
      FANSI_ind(i)
    );
    // nocov end
  }
}

/*
 * Strips ANSI tags from input
 *
 * Assumes input is NULL terminated.
 *
 * Since we do not use FANSI_read_next, we don't care about conversions to
 * UTF8.
 *
 * Code copied into trimws.c.
 *
 * Warn was used pre 1.0 to request to return warn info in attributes e.g. by
 * setting it to two, but that feature was dropped.
 */

SEXP FANSI_strip(SEXP x, SEXP ctl, SEXP warn) {
  if(TYPEOF(x) != STRSXP)
    error("Argument `x` should be a character vector.");  // nocov
  if(TYPEOF(ctl) != INTSXP)
    error("Internal Error: `ctl` should integer.");      // nocov

  R_xlen_t i, len = xlength(x);
  SEXP res_fin = x;

  PROTECT_INDEX ipx;
  // reserve spot if we need to alloc later
  PROTECT_WITH_INDEX(res_fin, &ipx);

  int any_ansi = 0;
  R_len_t mem_req = 0;          // how much memory we need for each ansi

  // Compute longest char element, we'll assume this is the required size of our
  // string buffer.  This is potentially wastful if there is one very large
  // CSI-less string and all the CSI strings are short.  The alternative would
  // be to keep reallocating to a larger size, maybe by 2x, but then that
  // requires the growing buffer, etc.  For now go simple

  for(i = 0; i < len; ++i) {
    FANSI_interrupt(i);
    R_len_t chr_len = LENGTH(STRING_ELT(x, i));
    if(chr_len > mem_req) mem_req = chr_len;
  }
  // Now strip
  char * chr_buff;
  const char * arg = "x";
  struct FANSI_state state;

  for(i = 0; i < len; ++i) {
    // Now full check
    if(!i) state = FANSI_state_init_ctl(x, warn, ctl, i);
    else FANSI_state_reinit(&state, x, i);

    SEXP x_chr = STRING_ELT(x, i);
    if(x_chr == NA_STRING) continue;
    FANSI_interrupt(i);

    int has_ctl = 0;
    const char * chr = CHAR(x_chr);
    const char * chr_track = chr;
    char * res_track = NULL, * res_start = NULL;

    // We re-use the allocated buffer for every string in the character
    // vector, which is why we re-assign to chr_buff here.  chr_buff will be
    // allocated in the loop below the first time it is needed, but we need to
    // re-assign re_start / res_track.
    res_start = res_track = chr_buff;

    struct FANSI_position pos_prev = state.pos;

    while(state.string[state.pos.x]) {
      int pos = FANSI_find_ctl(&state, i, arg);
      // This will also trigger for end-of-string if `has_ctl` is already true
      if(has_ctl || (state.status & CTL_MASK)) {
        has_ctl = 1;
        // As soon as we encounter ansi in any of the character vector elements,
        // allocate vector to track what has ansi
        if(!any_ansi) {
          any_ansi = 1;

          // We need to allocate a result vector since we'll be stripping ANSI
          // pos, and also the buffer we'll use to re-write the pos less strings
          REPROTECT(res_fin = duplicate(x), ipx);

          // Buffer is guaranteed to be an over-allocation, as it fits the
          // longest string in the vector, re-used for all strings.  It should
          // be no longer than R_LEN_T_MAX.
          chr_buff = (char *) R_alloc(((size_t) mem_req) + 1, sizeof(char));
          res_start = res_track = chr_buff;
        }
        int w_len = pos - pos_prev.x;
        memcpy(res_track, chr_track, w_len);
        res_track += w_len;
        chr_track = state.string + state.pos.x;
        pos_prev = state.pos;
    } }
    // Update string
    if(has_ctl) {
      // Copy final chunk if it exists because above we only memcpy when we
      // encounter the tag

      *res_track = '\0';
      FANSI_check_chr_size(res_start, res_track, i);
      SEXP chr_sexp = PROTECT(
        FANSI_mkChar0(res_start, res_track, getCharCE(x_chr), i)
      );
      SET_STRING_ELT(res_fin, i, chr_sexp);
      UNPROTECT(1);
    }
  }
  UNPROTECT(1);
  return res_fin;
}
static int is_special(char x) {
  return x != '\t' && x != '\n' && x >= 0 && x < 0x20 && x;
}
/*
 * Strips Extra ASCII Spaces
 *
 * Won't do anything about weird UTF8 spaces, etc.  Originally used to have the
 * option to handle ANSI ESC sequences and other escapes, but we ditched that in
 * favor of simplicity.  All those characters / sequences are treated as normal
 * characters, except for the newline and tab.
 *
 * Allows two spaces after periods, question marks, and exclamation marks.  This
 * is to line up with strwrap behavior.
 */

SEXP FANSI_process(
  SEXP input, SEXP term_cap, SEXP ctl, struct FANSI_buff *buff
) {
  if(TYPEOF(input) != STRSXP) error("Input is not a character vector.");

  PROTECT_INDEX ipx;
  SEXP res = input;
  // reserve spot if we need to alloc later
  int prt = 0;
  PROTECT_WITH_INDEX(res, &ipx); ++prt;
  SEXP R_true = PROTECT(ScalarLogical(1)); ++prt;
  SEXP R_zero = PROTECT(ScalarInteger(0)); ++prt;
  char * err_msg = "Processing whitespace";
  const char * arg = "x";

  int strip_any = 0;          // Have any elements in the STRSXP been stripped

  R_xlen_t len = XLENGTH(res);
  struct FANSI_state state;
  for(R_xlen_t i = 0; i < len; ++i) {
    FANSI_interrupt(i);
    // Don't warn - we don't modify or interpret sequences.  This allows bad
    // UTF-8 through.
    SEXP allowNA, keepNA, width, warn;
    allowNA = keepNA = R_true;
    width = warn = R_zero;
    if(!i) {
      // AFAICT process only for strwrap, and testing
      state = FANSI_state_init_full(
        input, warn, term_cap, allowNA, keepNA, width, ctl, i
      );
    } else FANSI_state_reinit(&state, input, i);

    const char * string = state.string;
    const char * string_start = string;

    int len_j = LENGTH(STRING_ELT(input, i)); // R_len_t checked to fit in int
    int strip_this, to_strip, to_strip_nl, punct_prev, punct_prev_prev,
        space_prev, space_start, para_start, newlines, newlines_start,
        has_tab_or_nl, leading_spaces, reset;

    strip_this = to_strip = to_strip_nl = punct_prev = punct_prev_prev =
      space_prev = space_start = newlines = newlines_start = has_tab_or_nl =
      reset = 0;

    para_start = leading_spaces = 1;

    int j_last = 0;

    // All spaces [ \t\n] are converted to spaces.  First space is kept, unless
    // right after [.?!][)\\"']{0,1}, in which case one more space can be kept.
    //
    // One exception is that sequences of spaces that resolve to more than one
    // newline are kept as a pair of newlines.
    //
    // We purposefully allow ourselves to read up to the NULL terminator.

    for(int j = 0; j <= len_j; ++j) {
      int newline = string[j] == '\n';
      int tab = string[j] == '\t';

      has_tab_or_nl += newline + tab;

      if(newline) {
        if(!newlines) {
          newlines_start = j;
          to_strip_nl = to_strip;  // how many chrs need stripping by first nl
        }
        ++newlines;
      }
      int space = ((string[j] == ' ') || tab || newline);
      int line_end = !string[j];

      // Need to keep track if we're in a sequence that starts with a space in
      // case a line ends, as normally we keep one or two spaces, but if we hit
      // the end of the line we don't want to keep them.
      if(space && !para_start) {
        if(!space_prev) space_start = 1;
        else if(space && space_prev && punct_prev_prev) space_start = 2;
      }
      // Anything we want to treat as a control is kept, and in the end will
      // be copied to the end of the string in question.

      int special = is_special(string[j]);
      int special_len = 0;

      if(special) { // Check that it is really special.
        int pos_prev = state.pos.x = j;
        FANSI_read_next(&state, i, arg);
        // Sequence is special if it is a recognized control
        if(state.status & CTL_ALL) {
          special_len = state.pos.x - pos_prev;
        } else {
          special = special_len = 0;
        }
      }
      // transcribe string if:
      if(
        // we've hit something that we don't need to strip, and we have accrued
        // characters to strip (more than one space, or more than two spaces if
        // preceeded by punct, or leading spaces
        (
          !space && !special && (
            (
              (to_strip && leading_spaces) ||
              (to_strip > 1 && (!punct_prev)) ||
              (to_strip > 2)
            ) ||
            has_tab_or_nl
        ) )
        ||
        // string end and we've already stripped previously or ending in spaces
        (line_end && (strip_this || space_start))
      ) {
        // need to copy entire STRSXP since we haven't done that yet
        if(!strip_any) {
          REPROTECT(res = duplicate(input), ipx);
          strip_any = 1;
        }
        // Make sure buffer is big enough (could be too big)
        if(!strip_this) {
          FANSI_size_buff0(buff, len_j);
          strip_this = 1;
        }
        // newlines normally act as spaces, but if there are two or more in a
        // sequence of tabs/spaces then they behave like a paragraph break
        // so we will replace that sequence with two newlines;

        const char * spc_chr = " ";
        int copy_to = j;
        int to_strip0 = to_strip;

        if(newlines > 1) {
          copy_to = newlines_start;
          space_start = 2;
          to_strip = to_strip_nl; // how many chars to strip by first newline
          spc_chr = "\n";
        }
        // Copy the portion up to the point we know should be copied, will add
        // back spaces and/or newlines as needed.  This does not skip specials,
        // just delays them!

        int copy_bytes =
          copy_to -      // current position
          j_last -       // less last time we copied
          to_strip;      // less extra stuff to strip

        if(copy_bytes) {
          FANSI_W_MCOPY(buff, string_start, copy_bytes);
        }
        // Instead of all the trailing spaces etc we skip, write one or two
        // spaces or newlines as needed.
        if(!line_end) {
          if(space_start) FANSI_W_COPY(buff, spc_chr);
          if(space_start > 1) FANSI_W_COPY(buff, spc_chr);
        }
        // Anything that is not a space/tab/nl that was considered non-breaking
        // with respect to trailing white space should be copied at end
        // otherwise unmodified.
        int copy_end = j_last + copy_bytes;
        for(int k = copy_end; k < copy_end + to_strip0; ++k) {
          if(is_special(string[k])) {
            state.pos.x = k;
            FANSI_read_next(&state, i, arg);
            int bytes = state.pos.x - k;
            FANSI_W_MCOPY(buff, string + k, bytes);
            k += bytes - 1;
          }
        }
        // Preprare for next sequence
        string_start = string + j;
        j_last = j;
        reset = 1;
      } else if(space) {
        to_strip++;
      } else if(special) {
        // treat special like a space, but only if preceded by space
        if(space_prev) {
          to_strip += special_len;
          space = 1;
        }
        j += special_len - 1;
      } else {
        reset = 1;
      }
      // We ended streak of spaces/etc so, reset
      if(reset) {
        reset = 0;
        to_strip = space_start = newlines = has_tab_or_nl = leading_spaces = 0;
      }
      para_start = newlines > 1;
      space_prev = space;
      punct_prev_prev = punct_prev || (special && punct_prev_prev);

      // To match what `strwrap` does, we treat as punctuation [.?!], and also
      // treat them as punctuation if they are followed by closing quotes or
      // parens.
      punct_prev =
        (string[j] == '.' || string[j] == '!' || string[j] == '?') ||
        (
          punct_prev &&
          (string[j] == '"' || string[j] == '\'' || string[j] == ')')
        );
    }
    if(strip_this) {
      SEXP chrsxp = PROTECT(
        FANSI_mkChar0(
          buff->buff0, buff->buff, getCharCE(STRING_ELT(input, i)), i
      ) );
      SET_STRING_ELT(res, i, chrsxp);
      UNPROTECT(1);
    }
  }
  UNPROTECT(prt);
  return res;
}

SEXP FANSI_process_ext(SEXP input, SEXP term_cap, SEXP ctl) {
  struct FANSI_buff buff;
  FANSI_INIT_BUFF(&buff);
  SEXP res = PROTECT(FANSI_process(input, term_cap, ctl, &buff));
  FANSI_release_buff(&buff, 1);
  UNPROTECT(1);
  return res;
}
