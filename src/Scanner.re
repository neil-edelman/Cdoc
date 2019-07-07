/** 2019 Neil Edelman, distributed under the terms of the MIT License,
 \url{ https://opensource.org/licenses/MIT }.

 This is a context-sensitive parser. Lexes C documents on-line and (partially)
 parses them to give the state, (comments, code, strings, character literals,
 documentation,) the indent level, etc. (Imperfectly, but good enough?)

 @title Scanner.re
 @author Neil
 @version 2019-06
 @std C89
 @depend re2c (\url{http://re2c.org/})
 @fixme Different doc comments need new paragraphs.
 @fixme Lists in comments, etc.
 @fixme {void A_BI_(Create, Thing)(void)} -> {<A>Create<BI>Thing(void)}.
 @fixme Trigraph support, (haha.)
 @fixme Old-style function support. */

#include <stdio.h>  /* printf */
#include <string.h> /* memset strchr */
/* #define NDEBUG */
#include <assert.h> /* assert */
#include <limits.h> /* INT_MAX */
#include <errno.h>  /* errno EILSEQ */

#include "../src/Scanner.h"

static const char *const states[] = { STATE(STRINGISE3_A) };
static const int state_is_doc[] = { STATE(PARAM3_C) };

static void state_to_string(const enum State *s, char (*const a)[12]) {
	strncpy(*a, states[*s], sizeof *a - 1);
	(*a)[sizeof *a - 1] = '\0';
}

/* Define {StateArray}, a stack of states. */
#define ARRAY_NAME State
#define ARRAY_TYPE enum State
#define ARRAY_TO_STRING &state_to_string
#define ARRAY_STACK
#include "../src/Array.h"

/* Define {CharArray}, a vector of characters -- dynamic string. */
#define ARRAY_NAME Char
#define ARRAY_TYPE char
#include "../src/Array.h"



/* Define QUOTE. */
#ifdef QUOTE
#undef QUOTE
#endif
#ifdef QUOTE_
#undef QUOTE_
#endif
#define QUOTE_(name) #name
#define QUOTE(name) QUOTE_(name)

/*!types:re2c*/

/*!re2c
re2c:yyfill:enable = 0;
re2c:define:YYCTYPE  = char;
re2c:define:YYFILL   = "return END;";
re2c:define:YYFILL:naked = 1;
re2c:define:YYLIMIT  = scanner.limit; // fixme: what is this?
re2c:define:YYCURSOR = scanner.cursor;
re2c:define:YYMARKER = scanner.marker; // Rules overlap.
re2c:define:YYCTXMARKER = scanner.ctx_marker;
re2c:define:YYCONDTYPE = "ScanState";
re2c:define:YYGETCONDITION = "scanner.condition";
re2c:define:YYGETCONDITION:naked = 1;
re2c:define:YYSETCONDITION = "scanner.condition = @@;";
re2c:define:YYSETCONDITION:naked = 1;
*/

/** Scanner reads a file and extracts semantic information. */
struct Scanner {
	/* `buffer` {re2c} variables. These point directly into {buffer} so no
	 modifying. */
	const char *limit, *cursor, *marker, *ctx_marker, *from;
	/* Weird {c2re} stuff: these fields have to come after when >5? */
	struct CharArray buffer, marks;
	struct StateArray states;
	enum ScanState condition;
	enum Symbol symbol;
	int indent_level, doc_indent_level;
	int ignore_block;
	size_t line, doc_line;
} scanner;


/** Unloads Scanner from memory. */
void Scanner_(void) {
	CharArray_(&scanner.buffer);
	CharArray_(&scanner.marks);
	StateArray_(&scanner.states);
	scanner.condition = yyccode; /* Generated by `re2c`. */
	scanner.symbol = END;
	scanner.indent_level = scanner.doc_indent_level = 0;
	scanner.ignore_block = 0;
	scanner.line = scanner.doc_line = 0;
	scanner.limit = scanner.cursor = scanner.marker = scanner.ctx_marker
		= scanner.from = 0;
}

/* Have {re2c} generate {YYMAXFILL}.
 \url{ http://re2c.org/examples/example_02.html }. */
/*!max:re2c*/

/** New Scanner. Reads from `stdin` until done, (it doesn't make sense to
 call this twice.)
 @return Success.
 @throws malloc free fread */
int Scanner(void) {
	const size_t granularity = 1024;
	char *read_here;
	size_t nread;
	/* Read all contents from `stdin` at once. */
	do {
		if(!(read_here = CharArrayBuffer(&scanner.buffer, granularity))
			|| (nread = fread(read_here, 1, granularity, stdin), ferror(stdin))
			|| !CharArrayAddSize(&scanner.buffer, nread)) goto catch;
	} while(nread == granularity);
	/* Fill the past the file with '\0' for fast lexing, I think? */
	if(!(read_here = CharArrayBuffer(&scanner.buffer, YYMAXFILL))
		|| !(memset(read_here, '\0', YYMAXFILL),
		CharArrayAddSize(&scanner.buffer, YYMAXFILL))) goto catch;
	scanner.condition = yyccode;
	/* Point these toward the first char; `buffer` is necessarily done
	 growing, or we could not do this. */
	/* fixme: is this right? */
	scanner.limit = CharArrayBack(&scanner.buffer, 0) + 1;
	scanner.cursor = scanner.marker = scanner.ctx_marker = scanner.from
		= CharArrayGet(&scanner.buffer);
	scanner.line = scanner.doc_line = 1;
	return 1;
catch:
	Scanner_();
	return 0;
}

static void debug(void) {
	int indent;
	printf("%lu:\t", (unsigned long)scanner.line);
	for(indent = 0; indent < scanner.indent_level; indent++)
		fputc('\t', stdout);
	printf("%s %s \"%.*s\", ignore %d, indent %d\n",
		StateArrayToString(&scanner.states),
		symbols[scanner.symbol], (int)(scanner.cursor - scanner.from),
		scanner.from, scanner.ignore_block, scanner.indent_level);
}

void ScannerPrintState(void) {
	debug();
}

/** Scans. */
static enum Symbol scan(void) {
	scanner.doc_line = scanner.line;
reset:
	scanner.from = scanner.cursor;
scan:
/*!re2c
	whitespace = [ \t\v\f];
	newline = "\n" | "\r" "\n"?;
	art = "*"? whitespace* newline " *";
	wordchars = [^ \t\n\v\f\r\\@&<>_`,{}]; // Why @?
	doc = "/""**";
	begin_comment = "/""*";
	end_comment = "*""/";
	cxx_comment = "//" [^\n]*;
	macro = ("#" | "%:");
	// @fixme No trigraph support.
	// char_type = "u8"|"u"|"U"|"L"; <- These get caught in id; don't care.
	oct = "0" [0-7]*;
	dec = [1-9][0-9]*;
	hex = '0x' [0-9a-fA-F]+;
	frc = [0-9]* "." [0-9]+ | [0-9]+ ".";
	exp = 'e' [+-]? [0-9]+;
	flt = (frc exp? | [0-9]+ exp) [fFlL]?;
	number = (oct | dec | hex | flt) [uUlL]*;
	operator = ":" | "..." | "::" | "?" | "+" | "-" | "*" | "/" | "%" | "^"
		| "xor" | "&" | "bitand" | "|" | "bitor" | "~" | "compl" | "!" | "not"
		| "=" | "<" | ">" | "+=" | "-=" | "%=" | "^=" | "xor_eq"
		| "&=" | "and_eq" | "|=" | "or_eq" | "<<" | ">>" | ">>=" | "<<="
		| "!=" | "not_eq" | "<=" | ">=" | "&&" | "and" | "||" | "or" | "++"
		| "--" | "." | "->";
	// Extension (hack) for generic macros; if one names them this way, it will
	// be documented nicely; the down side is, these are legal names for
	// identifiers; will be confused if you name anything this way that IS an
	// identifier.
	generic = [A-Z]+ "_";
	// Supports only C90 ids. That would be complicated.
	id = [a-zA-Z_][a-zA-Z_0-9]*;

	/////////////////////////////////////////////////////////////////

	<doc, param, math, code> * { printf("Unknown, <%.*s>; ignored.\n",
		(int)(scanner.cursor - scanner.from), scanner.from); goto reset; }
	<comment, macro_comment, string, character, macro> * { goto reset; }
	// http://re2c.org/examples/example_03.html
	<code> "\x00" { if(scanner.limit - scanner.cursor <= YYMAXFILL) return END;
		goto reset; }
	<doc, param, math, comment, macro_comment, string, character, macro> "\x00"
		{ if(scanner.limit - scanner.cursor <= YYMAXFILL)
		return errno = EILSEQ, END; goto reset; }

	<doc, param, math, code, macro> whitespace+ { goto reset; }
	<code, macro> cxx_comment { goto reset; }

	<doc, param, math, code, comment, macro_comment> newline {
		scanner.line++;
		if(scanner.condition == yycdoc) return NEWLINE;
		goto reset;
	}
	<string, character> newline {
		return errno = EILSEQ, END;
	}
	<macro> newline {
		scanner.line++;
		scanner.condition = yyccode;
		goto reset;
	}
	// Continuation.
	<string, character, macro> "\\" newline { scanner.line++; goto scan; }
	<doc> art / [^/] { scanner.line++;
		if(scanner.condition == yycdoc) return NEWLINE; goto reset; }

	<doc> "*""/" :=> code
	<param, math> "*""/" { fprintf(stderr, "Math mode at end of comment.\n");
		return errno = EILSEQ, END; }

	<doc> (wordchars | ",")* { return WORD; }
	<param> (wordchars | "_" | "`" | "\\")* { return WORD; }
	<param> "," { return DOC_COMMA; }
	<math> (wordchars | "," | "_")* { return WORD; }

	<doc, param, math> "\\\\" { return ESCAPED_BACKSLASH; }
	<doc, param, math> "\\`" { return ESCAPED_BACKQUOTE; }
	<doc, param, math> "\\@" | "@" { return ESCAPED_EACH; }
	<doc, param, math> "\\_" { return ESCAPED_UNDERSCORE; }
	<doc> "_" { return ITALICS; }
	<doc> "`" :=> math
	<math> "`" :=> doc
	<doc> "{" :=> param
	<doc> "}" { fprintf(stderr, "Misplaced } in documentation.\n");
		return errno = EILSEQ, END; }
	<param> "{" { fprintf(stderr, "Misplaced { in documentation.\n");
		return errno = EILSEQ, END; }
	<param> "}" :=> doc

	// These are recognised in the documentation as stuff.
	<doc> "\\url" { return URL; }
	<doc> "\\cite" { return CITE; }
	<doc> "\\see" { return SEE; }

	// These are tags.
	<doc> "@title" { return TAG_TITLE; }
	<doc> "@param" { return TAG_PARAM; }
	<doc> "@author" { return TAG_AUTHOR; }
	<doc> "@std" { return TAG_STD; }
	<doc> "@depend" { return TAG_DEPEND; }
	<doc> "@version" { return TAG_VERSION; }
	<doc> "@since" { return TAG_SINCE; }
	<doc> "@fixme" { return TAG_FIXME; }
	<doc> "@deprecated" { return TAG_DEPRICATED; }
	<doc> "@return" { return TAG_RETURN; }
	<doc> "@throws" { return TAG_THROWS; }
	<doc> "@implements" { return TAG_IMPLEMENTS; }
	<doc> "@order" { return TAG_ORDER; }
	<doc> "@allow" { return TAG_ALLOW; }

	// Also escape these for {HTML}.
	<doc, param, math> "&" { return HTML_AMP; }
	<doc, param, math> "<" { return HTML_LT; }
	<doc, param, math> ">" { return HTML_GT; }

	<code> doc / [^/] :=> doc
	// With flattening the `ScanState` stack, this is not actually worth
	// the effort; honestly, it's not going to matter.
	<macro> doc / [^/] {
		fprintf(stderr, "Documentation comment inside macro.\n");
		debug();
		return errno = EILSEQ, END;
	}
	<code> begin_comment :=> comment
	<marco> begin_comment :=> macro_comment
	<code> macro :=> macro
	<code> "L"? "\"" :=> string
	<code> "'" :=> character
	<code> number { return CONSTANT; }
	<code> operator { return OPERATOR; }
	<code> generic { return ID_ONE_GENERIC; }
	<code> generic generic { return ID_TWO_GENERICS; }
	<code> generic generic generic { return ID_THREE_GENERICS; }
	<code> "struct" { return STRUCT; }
	<code> "union"      { return UNION; }
	<code> "enum"       { return ENUM; }
	<code> "typedef"    { return TYPEDEF; }
	<code> "static"     { return STATIC; }
	<code> "void"       { return VOID; }
	<code> ("{" | "<%") { scanner.indent_level++; return LBRACE; }
	<code> ("}" | "%>") { scanner.indent_level--; return RBRACE; }
	<code> ("[" | "<:") { return LBRACK; }
	<code> ("]" | ":>") { return RBRACK; }
	<code> "("          { return LPAREN; }
	<code> ")"          { return RPAREN; }
	<code> ","          { return COMMA; }
	<code> ";"          { return SEMI; }
	<code> id { return ID; }

	<comment> end_comment :=> code
	<macro_comment> end_comment :=> macro

	<string> "\"" { scanner.condition = yyccode; return CONSTANT; }
	<character> "'" { scanner.condition = yyccode; return CONSTANT; }
	// All additional chars are not escaped.
	<string, character> "\\". { goto scan; }
*/
}

/** Lexes the next token. This will update `ScannerToken` and
 `ScannerTokenInfo`.
 @return If the scanner had more tokens.
 @fixme Is it really neccesary to have docs inside of functions? This would be
 so much easier logic without. */
int ScannerNext(void) {
#if 0
	enum State state;
	/* Ignore a block of code if `scanner.ignore_block` is on. */
	do {
		if(!(state = state_look())
			|| !(scanner.symbol = state_fn[state]())) return 0;
		debug();
		/* Return everything that's not-code (_ie_ docs) in ignore-block. */
		if((state = state_look()) && state != CODE) return 1;
	} while(scanner.ignore_block && scanner.indent_level);
	/* Coming out of an ignore-block. */
	if(scanner.ignore_block) {
		assert(scanner.symbol == RBRACE);
		scanner.symbol = END_BLOCK;
		printf("Ignore-block ended.\n");
		scanner.ignore_block = 0;
	}
#endif
	return !!(scanner.symbol = scan());
}

/** We don't care what's in the function, just that it's a function. */
void ScannerIgnoreBlock(void) { scanner.ignore_block = 1; }

/** Fills `token` with the last token.
 @param{token} If null, does nothing. */
void ScannerToken(struct Token *const token) {
	if(!token) return;
	assert(/*scanner.symbol &&*/ scanner.from && scanner.from <= scanner.cursor);
	token->symbol = scanner.symbol;
	token->from = scanner.from;
	if(scanner.from + INT_MAX < scanner.cursor) {
		fprintf(stderr, "Length of string chopped to " QUOTE(INT_MAX) ".\n");
		token->length = INT_MAX;
	} else {
		token->length = (int)(scanner.cursor - scanner.from);
	}
	token->line = scanner.line;
}

/** Fills `info` with the last token information not stored in the token. */
void ScannerTokenInfo(struct TokenInfo *const info) {
	enum State state;
	if(!info) return;
	state = /*state_look()*/0;
	info->indent_level = scanner.indent_level;
	info->is_doc = state_is_doc[state];
	info->is_doc_far = scanner.doc_line + 2 < scanner.line;
	info->state = state;
}
