## Copyright (C) Brodie Gaslam
##
## This file is part of "fansi - ANSI Control Sequence Aware String Functions"
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 or 3 of the License.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## Go to <https://www.r-project.org/Licenses> for copies of the licenses.

#' Strip Control Sequences
#'
#' Removes _Control Sequences_ from strings.  By default it will
#' strip all known _Control Sequences_, including CSI/OSC sequences, two
#' character sequences starting with ESC, and all C0 control characters,
#' including newlines.  You can fine tune this behavior with the `ctl`
#' parameter.
#'
#' The `ctl` value contains the names of **non-overlapping** subsets of the
#' known _Control Sequences_ (e.g. "csi" does not contain "sgr", and "c0" does
#' not contain newlines).  The one exception is "all" which means strip every
#' known sequence.  If you combine "all" with any other options then everything
#' **but** those options will be stripped.
#'
#' @note Non-ASCII strings are converted to and returned in UTF-8 encoding.
#' @inheritParams substr_ctl
#' @inherit has_ctl seealso
#' @export
#' @param ctl character, any combination of the following values (see details):
#'   * "nl": strip newlines.
#'   * "c0": strip all other "C0" control characters (i.e. x01-x1f, x7F),
#'     except for newlines and the actual ESC character.
#'   * "sgr": strip ANSI CSI SGR sequences.
#'   * "csi": strip all non-SGR csi sequences.
#'   * "esc": strip all other escape sequences.
#'   * "all": all of the above, except when used in combination with any of the
#'     above, in which case it means "all but" (see details).
#' @param strip character, deprecated in favor of `ctl`.
#' @return character vector of same length as x with ANSI escape sequences
#'   stripped
#' @examples
#' string <- "hello\033k\033[45p world\n\033[31mgoodbye\a moon"
#' strip_ctl(string)
#' strip_ctl(string, c("nl", "c0", "sgr", "csi", "esc")) # equivalently
#' strip_ctl(string, "sgr")
#' strip_ctl(string, c("c0", "esc"))
#'
#' ## everything but C0 controls, we need to specify "nl"
#' ## in addition to "c0" since "nl" is not part of "c0"
#' ## as far as the `strip` argument is concerned
#' strip_ctl(string, c("all", "nl", "c0"))

strip_ctl <- function(x, ctl='all', warn=getOption('fansi.warn', TRUE), strip) {
  if(!missing(strip)) {
    message("Parameter `strip` has been deprecated; use `ctl` instead.")
    ctl <- strip
  }
  ## modifies / creates NEW VARS in fun env
  VAL_IN_ENV(x=x, ctl=ctl, warn=warn, warn.mask=get_warn_worst())

  if(length(ctl)) .Call(FANSI_strip_csi, x, CTL.INT, WARN.INT)
  else x
}
#' Strip Control Sequences
#'
#' This function is deprecated in favor of the [`strip_ctl`].  It
#' strips CSI SGR and OSC hyperlink sequences.
#'
#' @inheritParams strip_ctl
#' @inherit strip_ctl return
#' @keywords internal
#' @export
#' @examples
#' ## convenience function, same as `strip_ctl(ctl=c('sgr', 'url'))`
#' string <- "hello\033k\033[45p world\n\033[31mgoodbye\a moon"
#' strip_sgr(string)

strip_sgr <- function(x, warn=getOption('fansi.warn', TRUE)) {
  ## modifies / creates NEW VARS in fun env
  VAL_IN_ENV(x=x, warn=warn, warn.mask=get_warn_worst())
  ctl.int <- match(c("sgr", "url"), VALID.CTL)
  .Call(FANSI_strip_csi, x, ctl.int, WARN.INT)
}

#' Check for Presence of Control Sequences
#'
#' `has_ctl` checks for any _Control Sequence_.  You can check for different
#' types of sequences with the `ctl` parameter.  Warnings are only emitted for
#' malformed CSI or OSC sequences.
#'
#' @export
#' @seealso [`?fansi`][fansi] for details on how _Control Sequences_ are
#'   interpreted, particularly if you are getting unexpected results,
#'   [`unhandled_ctl`] for detecting bad control sequences.
#' @inheritParams substr_ctl
#' @inheritParams strip_ctl
#' @param which character, deprecated in favor of `ctl`.
#' @return logical of same length as `x`; NA values in `x` result in NA values
#'   in return
#' @examples
#' has_ctl("hello world")
#' has_ctl("hello\nworld")
#' has_ctl("hello\nworld", "sgr")
#' has_ctl("hello\033[31mworld\033[m", "sgr")

has_ctl <- function(x, ctl='all', warn=getOption('fansi.warn', TRUE), which) {
  if(!missing(which)) {
    message("Parameter `which` has been deprecated; use `ctl` instead.")
    ctl <- which
  }
  ## modifies / creates NEW VARS in fun env
  VAL_IN_ENV(x=x, ctl=ctl, warn=warn, warn.mask=get_warn_mangled())
  if(length(CTL.INT)) {
    .Call(FANSI_has_csi, x, CTL.INT, WARN.INT)
  } else rep(FALSE, length(x))
}
#' Check for Presence of Control Sequences
#'
#' This function is deprecated in favor of the [`has_ctl`].  It
#' checks for CSI SGR and OSC hyperlink sequences.
#'
#' @inheritParams has_ctl
#' @inherit has_ctl return
#' @keywords internal
#' @export

has_sgr <- function(x, warn=getOption('fansi.warn', TRUE))
  has_ctl(x, ctl=c("sgr", "url"), warn=warn)

#' Utilities for Managing CSI and OSC State  In Strings
#'
#' `state_at_end` reads through strings computing the accumulated SGR and
#' OSC hyperlinks, and outputs the active state at the end of them.
#' `close_state` produces the sequence that closes any SGR active and OSC
#' hyperlinks at the end of each input string.  If `normalize = FALSE`
#' (default), it will emit the reset code "ESC&lbrack;0m" if any SGR is present.
#' It is more interesting for closing SGRs if `normalize = TRUE`.  Unlike
#' `state_at_end` and other functions `close_state` has no concept of `carry`:
#' it will only emit closing sequences for states explicitly active at the end
#' of a string.
#'
#' @export
#' @inheritParams substr_ctl
#' @inheritSection substr_ctl Control and Special Sequences
#' @inheritSection substr_ctl Output Stability
#' @inherit has_ctl seealso
#' @return character vector same length as `x`.
#' @examples
#' x <- c("\033[44mhello", "\033[33mworld")
#' state_at_end(x)
#' state_at_end(x, carry=TRUE)
#' (close <- close_state(state_at_end(x, carry=TRUE), normalize=TRUE))
#' writeLines(paste0(x, close, " no style"))

state_at_end <- function(
  x,
  warn=getOption('fansi.warn', TRUE),
  term.cap=getOption('fansi.term.cap', dflt_term_cap()),
  normalize=getOption('fansi.normalize', FALSE),
  carry=getOption('fansi.carry', FALSE)
) {
  ## modifies / creates NEW VARS in fun env
  VAL_IN_ENV(x=x, ctl='sgr', warn=warn, term.cap=term.cap, carry=carry)
  .Call(
    FANSI_state_at_end,
    x,
    WARN.INT,
    TERM.CAP.INT,
    CTL.INT,
    normalize,
    carry,
    "x",
    TRUE  # allowNA
  )
}
# Given an SGR, compute the sequence that closes it

#' @export
#' @rdname state_at_end

close_state <- function(
  x,
  warn=getOption('fansi.warn', TRUE),
  normalize=getOption('fansi.normalize', FALSE)
) {
  ## modifies / creates NEW VARS in fun env
  VAL_IN_ENV(x=x, warn=warn, normalize=normalize)
  .Call(FANSI_close_state, x, WARN.INT, 1L, normalize)
}


## Process String by Removing Unwanted Characters
##
## This is to simulate what `strwrap` does, exposed for testing purposes.

process <- function(x, ctl="all")
  .Call(
    FANSI_process, enc_to_utf8(x), 1L, match(ctl, VALID.CTL)
  )

