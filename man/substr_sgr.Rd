% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/substr2.R
\name{substr_sgr}
\alias{substr_sgr}
\alias{substr2_sgr}
\title{SGR Control Sequence Aware Version of substr}
\usage{
substr_sgr(
  x,
  start,
  stop,
  warn = getOption("fansi.warn", TRUE),
  term.cap = getOption("fansi.term.cap", dflt_term_cap()),
  normalize = getOption("fansi.normalize", FALSE),
  carry = getOption("fansi.carry", FALSE),
  terminate = getOption("fansi.terminate", TRUE)
)

substr2_sgr(
  x,
  start,
  stop,
  type = "chars",
  round = "start",
  tabs.as.spaces = getOption("fansi.tabs.as.spaces", FALSE),
  tab.stops = getOption("fansi.tab.stops", 8L),
  warn = getOption("fansi.warn", TRUE),
  term.cap = getOption("fansi.term.cap", dflt_term_cap()),
  normalize = getOption("fansi.normalize", FALSE),
  carry = getOption("fansi.carry", FALSE),
  terminate = getOption("fansi.terminate", TRUE)
)
}
\arguments{
\item{x}{a character vector or object that can be coerced to such.}

\item{start}{integer.  The first element to be extracted or replaced.}

\item{stop}{integer.  The first element to be extracted or replaced.}

\item{warn}{TRUE (default) or FALSE, whether to warn when potentially
problematic \emph{Control Sequences} are encountered.  These could cause the
assumptions \code{fansi} makes about how strings are rendered on your display
to be incorrect, for example by moving the cursor (see \code{\link[=fansi]{?fansi}}).
At most one warning will be issued per element in each input vector.  Will
also warn about some badly encoded UTF-8 strings, but a lack of UTF-8
warnings is not a guarantee of correct encoding (use \code{\link{validUTF8}} for
that).}

\item{term.cap}{character a vector of the capabilities of the terminal, can
be any combination of "bright" (SGR codes 90-97, 100-107), "256" (SGR codes
starting with "38;5" or "48;5"), "truecolor" (SGR codes starting with
"38;2" or "48;2"), and "all". "all" behaves as it does for the \code{ctl}
parameter: "all" combined with any other value means all terminal
capabilities except that one.  \code{fansi} will warn if it encounters SGR codes
that exceed the terminal capabilities specified (see \code{\link{term_cap_test}}
for details).  In versions prior to 1.0, \code{fansi} would also skip exceeding
SGRs entirely instead of interpreting them.  You may add the string "old"
to any otherwise valid \code{term.cap} spec to restore the pre 1.0 behavior.
"old" will not interact with "all" the way other valid values for this
parameter do.}

\item{normalize}{TRUE or FALSE (default) whether SGR sequence should be
normalized out such that there is one distinct sequence for each SGR code.
normalized strings will occupy more space (e.g. "\033[31;42m" becomes
"\033[31m\033[42m"), but will work better with code that assumes each SGR
code will be in its own escape as \code{crayon} does.}

\item{carry}{TRUE, FALSE (default), or a scalar string, controls whether to
interpret the character vector as a "single document" (TRUE or string) or
as independent elements (FALSE).  In "single document" mode, active state
at the end of an input element is considered active at the beginning of the
next vector element, simulating what happens with a document with active
state at the end of a line.  If FALSE each vector element is interpreted as
if there were no active state when it begins.  If character, then the
active state at the end of the \code{carry} string is carried into the first
element of \code{x} (see "Replacement Functions" for differences there).  The
carried state is injected in the interstice between an imaginary zeroeth
character and the first character of a vector element.  See the "Position
Semantics" section of \code{\link{substr_ctl}} and the "State Interactions" section
of \code{\link[=fansi]{?fansi}} for details.  Except for \code{\link{strwrap_ctl}} where \code{NA} is
treated as the string \code{"NA"}, \code{carry} will cause \code{NA}s in inputs to
propagate through the remaining vector elements.}

\item{terminate}{TRUE (default) or FALSE whether substrings should have
active state closed to avoid it bleeding into other strings they may be
prepended onto.  This does not stop state from carrying if \code{carry = TRUE}.
See the "State Interactions" section of \code{\link[=fansi]{?fansi}} for details.}

\item{type}{character(1L) partial matching
\code{c("chars", "width", "graphemes")}, although types other than "chars" only
work correctly with R >= 3.2.2.  See \code{\link[base:nchar]{?nchar}}.}

\item{round}{character(1L) partial matching
\code{c("start", "stop", "both", "neither")}, controls how to resolve
ambiguities when a \code{start} or \code{stop} value in "width" \code{type} mode falls
within a wide display character.  See details.}

\item{tabs.as.spaces}{FALSE (default) or TRUE, whether to convert tabs to
spaces (and supress tab related warnings).  This can only be set to TRUE if
\code{strip.spaces} is FALSE.}

\item{tab.stops}{integer(1:n) indicating position of tab stops to use
when converting tabs to spaces.  If there are more tabs in a line than
defined tab stops the last tab stop is re-used.  For the purposes of
applying tab stops, each input line is considered a line and the character
count begins from the beginning of the input line.}
}
\value{
A character vector of the same length and with the same attributes as
x (after possible coercion and re-encoding to UTF-8).
}
\description{
These functions are deprecated in favor of the \code{\link{substr_ctl}} flavors.
}
\keyword{internal}
