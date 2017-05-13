/* HTTP protocol grammar fragments.
 *
 * Mostly in the form of regular expressions. Where possible, anyway.
 *
 * See also:
 * * Project Documentation: https://ef.gy/documentation/cxxhttp
 * * Project Source Code: https://github.com/ef-gy/cxxhttp
 * * Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 *
 * @copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 */
#if !defined(CXXHTTP_HTTP_GRAMMAR_H)
#define CXXHTTP_HTTP_GRAMMAR_H

#include <string>

namespace cxxhttp {
namespace http {
namespace grammar {
// from RFC 5234
// https://tools.ietf.org/html/rfc5234#appendix-B.1

// rules that are not separate constants, because they're already part of the
// C++ syntax:
//
//          CR             =  %x0D  ; carriage return
//          CRLF           =  CR LF  ; Internet standard newline
//          DQUOTE         =  %x22  ; " (Double Quote)
//          HTAB           =  %x09  ; horizontal tab
//          LF             =  %x0A  ; linefeed
//          SP             =  %x20

/* Alphabet character.
 *
 * Any uppercase or lowercase character used in English, without diacritics.
 *
 * Original grammar rule:
 *          ALPHA          =  %x41-5A / %x61-7A  ; A-Z / a-z
 */
static const std::string alpha = "[A-Za-z]";

/* Numerical digit.
 *
 * Any of the 10 digits used in English.
 *
 * Original grammar rule:
 *          DIGIT          =  %x30-39  ; 0-9
 */
static const std::string digit = "[0-9]";

/* Any 8-bit character.
 *
 * For when we're really not being picky.
 *
 * Original grammar rule:
 *          OCTET          =  %x00-FF  ; 8 bits of data
 */
static const std::string octet = "[\\\x00-\\\xff]";

//          VCHAR          =  %x21-7E  ; visible (printing) characters
static const std::string vchar = "[!-\\\x7e]";

//          WSP            =  SP / HTAB  ; white space
static const std::string wsp = "[ \t]";

// from RFC 7230
// https://tools.ietf.org/html/rfc7230#appendix-B
//
// Note the extra syntax rule from chapter 7:
//
//     1#element => element *( OWS "," OWS element )
//     #element => [ 1#element ]
//     <n>#<m>element => element <n-1>*<m-1>( OWS "," OWS element )
//
// Most of this is defined in section 3, "Message Format" which opens with this
// overall ABNS rule:
//
//      HTTP-message   = start-line
//                       *( header-field CRLF )
//                       CRLF
//                       [ message-body ]
//

/* Protocol name.
 *
 * From RFC 7230, section 2.6, "Protocol Versioning".
 *
 * Used in the start line, and is a verbatim, upper-case "HTTP". Only here for
 * completeness, as this would be used via httpVersion, which has subexpressions
 * for getting the actual version digits.
 *
 * Original grammar rule:
 *
 *      HTTP-name     = %x48.54.54.50 ; "HTTP", case-sensitive
 */
static const std::string httpName = "HTTP";

/* Protocol version.
 *
 * HTTP uses a fixed, single-digit and two-component version numbering scheme.
 * This has subexpressions for the versions added, as these are the only parts
 * that could be different between protocol versions and we need this bit of
 * information only while parsing a status line.
 *
 * Original grammar rule:
 *
 *      HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
 */
static const std::string httpVersion = "HTTP/([0-9])\\.([0-9])";

// Whitespace rules:
// Only space and tab are considered whitespace.
//
//      OWS            = *( SP / HTAB )  ; optional whitespace
static const std::string ows = "[ \t]*";
//      RWS            = 1*( SP / HTAB )  ; required whitespace
static const std::string rws = "[ \t]+";
//      BWS            = OWS  ; "bad" whitespace
static const std::string bws = "[ \t]*";

/* HTTP "obsolete" text.
 *
 * It's considered good practice, according to RFC 7230, to not use anyting but
 * 7-bit ASCII in HTTP control contexts. However, the transport is expected to
 * be 8-bit, and for backwards compatibility with clients using other encodings
 * it's encouraged to be lenient about that.
 *
 * This character class represents all characters with the highest bit set in an
 * 8-bit encoding. Places where this is actually used will have this class
 * rolled into other character classes nearby.
 *
 * Original grammar rule:
 *
 *      obs-text       = %x80-FF
 */
static const std::string obsText = "[\\\x80-\\\xff]";

/* A single token character.
 *
 * This is any VCHAR, except delimiters. Has been resolved to a simple character
 * class to make it easy to capture with regex subexpressions.
 *
 * Original grammar rule:
 *
 *      tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *                     / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *                     / DIGIT / ALPHA
 */
static const std::string tchar = "[---!#$%&'*+.^_`|~0-9A-Za-z]";

/* An HTTP token.
 *
 * HTTP uses tokens for headers, methods and other places where an opaque name
 * of one form or another is needed.
 *
 * This has no subexpressions to make it easy to capture.
 *
 * Original grammar rule:
 *
 *      token          = 1*tchar
 */
static const std::string token = tchar + "+";

// RFC 7230, section 3.1: start line

//      start-line     = request-line / status-line
//      request-line   = method SP request-target SP HTTP-version CRLF
//      method         = token
//      status-line = HTTP-version SP status-code SP reason-phrase CRLF

/* Status code.
 *
 * Must be exactly 3 digits, no more and no less. This actually allows codes
 * outside of the "normal" range, but this is what the grammar says.
 *
 * This has no subexpressions, because we want this easily captured.
 *
 * Original grammar rule:
 *
 *      status-code    = 3DIGIT
 */
static const std::string statusCode = "[0-9]{3}";

/* Reason phrase.
 *
 * Basically a text explanation for the status code. Note that this grammar
 * allows entirely empty reaosn phrases, but there still must be a space
 * between a status code and an empty reason phrase.
 *
 * Original grammr rule:
 *
 *      reason-phrase  = *( HTAB / SP / VCHAR / obs-text )
 */
static const std::string reasonPhrase = "[\t !-\\\x7e\\\x80-\\\xff]*";

// RFC 7230, section 3.2: header lines

/* Quoted pair.
 *
 * Basically any VCHAR or obs-text, but prefixed by a backslash.
 *
 *      quoted-pair    = "\" ( HTAB / SP / VCHAR / obs-text )
 */
static const std::string quotedPair = "\\\\[\t !-\\\x7e\\\x80-\\\xff]";

/* Quoted text.
 *
 * This production basically matches VCHAR without backspaces or quotes, which
 * is important for quoted-string to work as expected as that in turn allows
 * those characters but only when prefixed with a backspace.
 *
 *      qdtext         = HTAB / SP /%x21 / %x23-5B / %x5D-7E / obs-text
 */
static const std::string qdtext = "[\t !#-,,-\\\x5b\\\x5d-\\\x7e\\\x80-\\\xff]";

//      quoted-string  = DQUOTE *( qdtext / quoted-pair ) DQUOTE
static const std::string quotedString =
    "(\"((" + qdtext + "|" + quotedPair + ")*)\")";

/* Comment text.
 *
 * Same avoidances as qdtext, but also avoids parentheses.
 *
 *      ctext          = HTAB / SP / %x21-27 / %x2A-5B / %x5D-7E / obs-text
 */
static const std::string ctext =
    "[\t !-\\\x27\\\x2a-\\\x5b\\\x5d-\\\x7e\\\x80-\\\xff]";

/* Comment with parentheses.
 *
 * Comments in header fields are basically anything within parentheses. The
 * problem with that is that nesting parentheses is not a thing you can do in
 * a regular language, so trying to use this will likely not yield the
 * correct result.
 * Let's hope it won't bite us in any critical headers that aren't User-Agent.
 *
 *      comment        = "(" *( ctext / quoted-pair / comment ) ")"
 */
static const std::string comment =
    "(\\(((" + ctext + "|" + quotedPair + "|[()])*)\\))";

//      header-field   = field-name ":" OWS field-value OWS
//
//      field-name     = token
static const std::string fieldName = token;

// This involves newlines, so we need to do this in actual code. Because it's
// easier to just pre-split per line and go from there.
//
//      field-value    = *( field-content / obs-fold )
//      obs-fold       = CRLF 1*( SP / HTAB ) ; obsolete line folding

//      field-vchar    = VCHAR / obs-text
static const std::string fieldVchar = "[!-\\\x7e\\\x80-\\\xff]";
static const std::string fieldVcharWS = "[ \t!-\\\x7e\\\x80-\\\xff]";

// I'm not sure this repeats as it was intended in the original grammar. I mean,
// I think it does if it's unrolled, but not sure that was the intended way of
// writing it.
//
//      field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
static const std::string fieldContent = fieldVchar + fieldVcharWS + "*";
}
}
}

#endif
