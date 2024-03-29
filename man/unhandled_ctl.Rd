% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/unhandled.R
\name{unhandled_ctl}
\alias{unhandled_ctl}
\title{Identify Unhandled Control Sequences}
\usage{
unhandled_ctl(x, term.cap = getOption("fansi.term.cap", dflt_term_cap()))
}
\arguments{
\item{x}{character vector}

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
}
\value{
Data frame with as many rows as there are unhandled escape
sequences and columns containing useful information for debugging the
problem.  See details.
}
\description{
Will return position and types of unhandled \emph{Control Sequences} in a
character vector.  Unhandled sequences may cause \code{fansi} to interpret strings
in a way different to your display.  See \link{fansi} for details.  Functions that
interpret \emph{Special Sequences} (CSI SGR or OSC hyperlinks) might omit bad
\emph{Special Sequences} or some of their components in output substrings,
particularly if they are leading or trailing.  Some functions are more
tolerant of bad inputs than others.  For example \code{\link{nchar_ctl}} will not
report unsupported colors because it only cares about counts or widths.
\code{unhandled_ctl} will report all potentially problematic sequences.
}
\details{
To work around tabs present in input, you can use \code{\link{tabs_as_spaces}} or the
\code{tabs.as.spaces} parameter on functions that have it, or the \code{\link{strip_ctl}}
function to remove the troublesome sequences.  Alternatively, you can use
\code{warn=FALSE} to suppress the warnings.

This is a debugging function that is not optimized for speed and the precise
output of which might change with \code{fansi} versions.

The return value is a data frame with five columns:
\itemize{
\item index: integer the index in \code{x} with the unhandled sequence
\item start: integer the start position of the sequence (in characters)
\item stop: integer the end of the sequence (in characters), but note that if
there are multiple ESC sequences abutting each other they will all be
treated as one, even if some of those sequences are valid.
\item error: the reason why the sequence was not handled:
\itemize{
\item unknown-substring: SGR substring with a value that does not correspond
to a known SGR code or OSC hyperlink with unsupported parameters.
\item invalid-substr: SGR contains uncommon characters in ":<=>",
intermediate bytes, other invalid characters, or there is an invalid
subsequence (e.g. "ESC[38;2m" which should specify an RGB triplet
but does not).  OSCs contain invalid bytes, or OSC hyperlinks contain
otherwise valid OSC bytes in 0x08-0x0d.
\item exceed-term-cap: contains color codes not supported by the terminal
(see \link{term_cap_test}).  Bright colors with color codes in the 90-97 and
100-107 range in terminals that do not support them are not considered
errors, whereas 256 or truecolor codes in terminals that do not support
them are.  This is because the latter are often misinterpreted by
terminals that do not support them, whereas the former are typically
silently ignored.
\item CSI/OSC: a non-SGR CSI sequence, or non-hyperlink OSC sequence.
\item CSI/OSC-bad-substr: a CSI or OSC sequence containing invalid
characters.
\item malformed-CSI/OSC: a malformed CSI or OSC sequence, typically one that
never encounters its closing sequence before the end of a string.
\item non-CSI/OSC: a non-CSI or non-OSC escape sequence, i.e. one where the
ESC is followed by something other than "[" or "]".  Since we
assume all non-CSI sequences are only 2 characters long include the
ESC, this type of sequence is the most likely to cause problems as some
are not actually two characters long.
\item malformed-ESC: a malformed two byte ESC sequence (i.e. one not ending
in 0x40-0x7e).
\item C0: a "C0" control character (e.g. tab, bell, etc.).
\item malformed-UTF8: illegal UTF8 encoding.
\item non-ASCII: non-ASCII bytes in escape sequences.
}
\item translated: whether the string was translated to UTF-8, might be helpful in
odd cases were character offsets change depending on encoding.  You should
only worry about this if you cannot tie out the \code{start}/\code{stop} values to
the escape sequence shown.
\item esc: character the unhandled escape sequence
}
}
\note{
Non-ASCII strings are converted to UTF-8 encoding.
}
\examples{
string <- c(
  "\033[41mhello world\033[m", "foo\033[22>m", "\033[999mbar",
  "baz \033[31#3m", "a\033[31k", "hello\033m world"
)
unhandled_ctl(string)
}
\seealso{
\code{\link[=fansi]{?fansi}} for details on how \emph{Control Sequences} are
interpreted, particularly if you are getting unexpected results,
\code{\link{unhandled_ctl}} for detecting bad control sequences.
}
