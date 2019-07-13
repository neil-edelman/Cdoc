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

/* X-Marcos are great for debugging. */
#define PARAM(A) A
#define STRINGISE(A) #A
#define PARAM2_A(A, B) A
#define PARAM2_B(A, B) B
#define STRINGISE2_A(A, B) #A
#define PARAM3_A(A, B, C) A
#define PARAM3_B(A, B, C) B
#define PARAM3_C(A, B, C) C
#define STRINGISE3_A(A, B, C) #A

/* Define `Symbols` -- these are the numerical values given to a section of
 text. The prefix of symbols themselves have meaning; see \see{append}. */
#define SYMBOL(X) \
	/* EOF -- 0. */ \
	X(END, '~', 0), \
	/* `C` syntax. */ \
	X(C_OPERATOR, '*', &lit), X(C_COMMA, ',', &lit), X(C_SEMI, ';', &lit), \
	X(C_LBRACE, '{', &lit), X(C_RBRACE, '}', &lit), X(C_LPAREN, '(', &lit), \
	X(C_RPAREN, ')', &lit), X(C_LBRACK, '[', &lit), X(C_RBRACK, ']', &lit), \
	X(C_CONSTANT, '#', &lit), X(C_ID, 'x', &lit), \
	X(C_ID_ONE_GENERIC, '1', &gen1), X(C_ID_TWO_GENERICS, '2', &gen2), \
	X(C_ID_THREE_GENERICS, '3', &gen3), X(C_STRUCT, 's', &lit), \
	X(C_UNION, 's', &lit), X(C_ENUM, 's', &lit), X(C_TYPEDEF, 't', &lit), \
	X(C_STATIC, 'z', &lit), X(C_VOID, 'v', &lit), X(C_END_BLOCK, ';', 0), \
	/* Document syntax. */ \
	X(TAG_TITLE, '@', &lit), X(TAG_PARAM, '@', &lit), \
	X(TAG_AUTHOR, '@', &lit), X(TAG_STD, '@', &lit), X(TAG_DEPEND, '@', &lit), \
	X(TAG_VERSION, '@', &lit), X(TAG_SINCE, '@', &lit), \
	X(TAG_FIXME, '@', &lit), X(TAG_DEPRICATED, '@', &lit), \
	X(TAG_RETURN, '@', &lit), X(TAG_THROWS, '@', &lit), \
	X(TAG_IMPLEMENTS, '@', &lit), X(TAG_ORDER, '@', &lit), \
	X(TAG_ALLOW, '@', &lit), \
	/* Meaning/escapes document syntax. */ \
	X(DOC_BEGIN, '~', 0), X(DOC_END, '~', 0), X(DOC_WORD, 'w', &lit), \
	X(DOC_BACKSLASH, '\\', &esc_bs), X(DOC_BACKQUOTE, '\\', &esc_bq), \
	X(DOC_EACH, '\\', &esc_each), X(DOC_UNDERSCORE, '\\', &esc_under), \
	X(DOC_URL, '\\', &url), X(DOC_CITE, '\\', &cite), X(DOC_SEE, '\\', &see), \
	X(DOC_NEWLINE, 'n', &par), X(DOC_ITALICS, '_', &it), \
	/* Also do these from LaTeX to HTML. */ \
	X(DOC_HTML_AMP, '&', &esc_amp), X(DOC_HTML_LT, '&', &esc_lt), \
	X(DOC_HTML_GT, '&', &esc_gt), \
	/* Math/code `like this`. */ \
	X(MATH_BEGIN, '$', &math), X(MATH_END, '$', &math), \
	X(MATH_STUFF, 'y', &lit), \
	/* Variable lists {like, this}. */ \
	X(PARAM_BEGIN, '<', &lb), X(PARAM_END, '>', &rb), \
	X(PARAM_COMMA, '.', &lit), X(PARAM_ITEM, 'p', &lit)
enum Symbol { SYMBOL(PARAM3_A) };
static const char *const symbols[] = { SYMBOL(STRINGISE3_A) };
static const char symbol_mark[] = { SYMBOL(PARAM3_B) };


/** `Token` has a `Symbol` and is associated with an area of the text. */
struct Token {
	enum Symbol symbol;
	const char *from;
	int length;
	size_t line;
};
static void token_to_string(const struct Token *s, char (*const a)[12]) {
	/*int len = s->length >= 5 ? 5 : s->length;
	sprintf(*a, "%.4s<%.*s>", symbols[s->symbol], len, s->from);*/
	strncpy(*a, symbols[s->symbol], sizeof *a - 1);
	(*a)[sizeof *a - 1] = '\0';
}
#define ARRAY_NAME Token
#define ARRAY_TYPE struct Token
#define ARRAY_TO_STRING &token_to_string
#include "../src/Array.h"


/** `Tag` is a specific structure of array of `Token` representing @-tags. */
struct Tag {
	struct Token token;
	struct TokenArray header;
	struct TokenArray contents;
};
static void tag_to_string(const struct Tag *t, char (*const a)[12]) {
	strncpy(*a, symbols[t->token.symbol], sizeof *a - 1);
	(*a)[sizeof *a - 1] = '\0';
}
static void tag_init(struct Tag *const tag) {
	assert(tag);
	TokenArray(&tag->header);
	TokenArray(&tag->contents);
	/* FIXME!! Token(&tag->token);*/
}
#define ARRAY_NAME Tag
#define ARRAY_TYPE struct Tag
#define ARRAY_TO_STRING &tag_to_string
#include "../src/Array.h"
static void tag_array_remove(struct TagArray *const ta) {
	struct Tag *t;
	if(!ta) return;
	while((t = TagArrayPop(ta)))
		TokenArray_(&t->header), TokenArray_(&t->contents);
	TagArray_(ta);
}


/* Define the sections of output. */
#define SECTION(X) X(HEADER), X(DECLARATION), X(FUNCTION)
enum Section { SECTION(PARAM) };
static const char *const sections[] = { SECTION(STRINGISE) };


/** `Segment` is classified to a section of the document and can have
 documentation including tags and code. */
struct Segment {
	enum Section section;
	struct TokenArray doc, code;
	struct TagArray tags;
};
static void segment_init(struct Segment *const segment) {
	assert(segment);
	segment->section = HEADER; /* Default. */
	TokenArray(&segment->doc);
	TokenArray(&segment->code);
	TagArray(&segment->tags);
}
static void segment_to_string(const struct Segment *seg, char (*const a)[12]) {
	strncpy(*a, sections[seg->section], sizeof *a - 1);
	(*a)[sizeof *a - 1] = '\0';
}
#define ARRAY_NAME Segment
#define ARRAY_TYPE struct Segment
#define ARRAY_TO_STRING &segment_to_string
#include "../src/Array.h"


/** This is the top-level parser. */
static struct SegmentArray doc;

static void doc_(void) {
	struct Segment *seg;
	while((seg = SegmentArrayPop(&doc)))
		TokenArray_(&seg->doc), TokenArray_(&seg->code),
		tag_array_remove(&seg->tags);
	SegmentArray_(&doc);
}

/***********/

/* Define {CharArray}, a vector of characters -- dynamic string. */
#define ARRAY_NAME Char
#define ARRAY_TYPE char
#include "../src/Array.h"

/***********/

/* This defines `ScanState`; the trailing comma on an `enum` is not proper
 `C90`, hopefully they will fix it. */
/*!types:re2c*/

/** Scanner reads a file and extracts semantic information. */
struct Scanner {
	/* `buffer` {re2c} variables. These point directly into {buffer}. */
	const char *marker, *ctx_marker, *from, *cursor;
	/* Weird {c2re} stuff: these fields have to come after when >5? */
	struct CharArray buffer;
	enum ScanState state;
	enum Symbol symbol;
	int indent_level, doc_indent_level;
	int ignore_block;
	size_t line, doc_line;
} scanner;

/** Unloads Scanner from memory. */
static void Scanner_(void) {
	scanner.marker = scanner.ctx_marker = scanner.from = scanner.cursor = 0;
	CharArray_(&scanner.buffer);
	scanner.state = yyccode; /* Generated by `re2c`. */
	scanner.symbol = END;
	scanner.indent_level = scanner.doc_indent_level = 0;
	scanner.ignore_block = 0;
	scanner.line = scanner.doc_line = 0;
}

/** New Scanner. Reads from `stdin` until done; it doesn't make sense to
 call this twice since the input is consumed.
 @return Success.
 @throws{malloc, fread}
 @throws{EILSEQ} File has embedded nulls. */
static int Scanner(void) {
	const size_t granularity = 1024;
	char *read_here, *zero;
	size_t nread, zero_len;
	/* Read all contents from `stdin` at once. */
	do {
		if(!(read_here = CharArrayBuffer(&scanner.buffer, granularity))
			|| (nread = fread(read_here, 1, granularity, stdin), ferror(stdin))
			|| !CharArrayAddSize(&scanner.buffer, nread)) goto catch;
	} while(nread == granularity);
	/* Embed '\0' on the end for simple lexing. */
	if(!(zero = CharArrayNew(&scanner.buffer))) goto catch;
	*zero = '\0';
	scanner.state = yyccode;
	/* Point these toward the first char; `buffer` is necessarily done
	 growing, or we could not do this. */
	scanner.marker = scanner.ctx_marker = scanner.from = scanner.cursor
		= CharArrayGet(&scanner.buffer);
	scanner.line = scanner.doc_line = 1;
	/* We use simplified sentinel method of detecting EOF, so the file can have
	 no embedded '\0'. */
	{
		const char *const buffer = CharArrayGet(&scanner.buffer);
		zero_len = (size_t)(strchr(buffer, '\0') - buffer);
		if(zero_len != CharArraySize(&scanner.buffer) - 1)
			{ errno = EILSEQ; fprintf(stderr,
			"Expects Modified UTF-8 encoding; embedded '\\0' at byte %lu/%lu."
			"\n", (unsigned long)zero_len,
			(unsigned long)CharArraySize(&scanner.buffer) - 1); goto catch; }
	}
	return 1;
catch:
	Scanner_();
	return 0;
}

static void debug(void) {
	printf("Line %lu:%d: \"%.*s\" %s.\n", (unsigned long)scanner.line,
		scanner.indent_level, (int)(scanner.cursor - scanner.from),
		scanner.from, symbols[scanner.symbol]);
}

static void token_current(struct Token *const token, const enum Symbol symbol) {
	assert(token && scanner.from && scanner.from <= scanner.cursor);
	token->symbol = symbol;
	token->from = scanner.from;
	if(scanner.from + INT_MAX < scanner.cursor) {
		fprintf(stderr, "Length of token truncated to %d.\n", INT_MAX);
		token->length = INT_MAX;
	} else {
		token->length = (int)(scanner.cursor - scanner.from);
	}
	token->line = scanner.line;
}

/** This holds the sorting state information for `append`. */
static struct {
	struct Segment *segment;
	struct Tag *tag;
	struct TokenArray *current;
} sorter;

/*!re2c
re2c:yyfill:enable   = 0;
re2c:define:YYCTYPE  = char;
re2c:define:YYCURSOR = scanner.cursor;
*/

/** This appends the current token based on the global state in `sorter`.
 @return Success. */
static int append(const enum Symbol symbol) {
	struct Token *token = 0;
	enum { DISCARD, SORT_CODE, SORT_DOC, SORT_BEGIN_TAG, SORT_TAG }
		sort = DISCARD;
	const char *const symbol_string = symbols[symbol];
	assert(symbol);
	if(!sorter.segment && !(sorter.tag = 0, sorter.current = 0,
		sorter.segment = SegmentArrayNew(&doc))) return 0;
	/* Decide what to do based on the prefix. */
	switch(symbol_string[0]) {
	case 'C':
	case 'D':
	case 'T':
	case 'E':
	case 'M':
	case 'P':
	default:
		break;
	}
	
	if(!strncmp(symbols[symbol], "C_", 2)) {
	}
	switch(scanner.state) {
	case yyccode:
		assert(!strncmp(symbols[symbol], "C_", 2) || symbol == DOC_END);
		token = TokenArrayNew(&sorter.segment->code);
		break;
	case yycmath:
	case yycparam:
	case yycdoc:
		assert(!strncmp(symbols[symbol], "DOC_", 4)
			|| !strncmp(symbols[symbol], "TAG_", 4)
			|| !strncmp(symbols[symbol], "END_", 4)
			|| !strncmp(symbols[symbol], "MATH_", 4)
			|| !strncmp(symbols[symbol], "PARAM_", 6));
		token = TokenArrayNew(&sorter.segment->doc);
		break;
	default:
		/* `yyccharacter, yyccomment, yycmacro, yycmacro_comment, yycstring`
		 are not returning anything. */
		assert(0);
	}
	if(!token) return 0;
	token_current(token, symbol);
	return 1;
}

/*!re2c
re2c:yyfill:enable   = 0;
re2c:define:YYCTYPE  = char;
re2c:define:YYCURSOR = scanner.cursor;
re2c:define:YYMARKER = scanner.marker; // Rules overlap.
re2c:define:YYCTXMARKER = scanner.ctx_marker;
re2c:define:YYCONDTYPE = "ScanState";
re2c:define:YYGETCONDITION = "scanner.state";
re2c:define:YYGETCONDITION:naked = 1;
re2c:define:YYSETCONDITION = "scanner.state = @@;";
re2c:define:YYSETCONDITION:naked = 1;

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
	| "=" | "<" | ">" | "==" | "+=" | "-=" | "%=" | "^=" | "xor_eq"
	| "&=" | "and_eq" | "|=" | "or_eq" | "<<" | ">>" | ">>=" | "<<="
	| "!=" | "not_eq" | "<=" | ">=" | "&&" | "and" | "||" | "or" | "++"
	| "--" | "." | "->";
// Extension (hack) for generic macros; if one names them this way, it will
// be documented nicely; the down side is, these are legal names for
// identifiers; will be confused if you name anything this way that IS an
// identifier.
generic = [A-Z]+ "_";
// Supports only C90 ids. That would be complicated. I suppose you could hack
// it to accept a super-set?
id = [a-zA-Z_][a-zA-Z_0-9]*;
*/

/** Scans. */
static enum Symbol scan(void) {
	scanner.doc_line = scanner.line;
reset:
	scanner.from = scanner.cursor;
scan:
/*!re2c
	<doc, param, math, code> * { printf("Unknown, <%.*s>; ignored.\n",
		(int)(scanner.cursor - scanner.from), scanner.from); goto reset; }
	<comment, macro_comment, string, character, macro> * { goto reset; }
	// http://re2c.org/examples/example_03.html
	<code> "\x00" { return END; }
	<doc, param, math, comment, macro_comment, string, character, macro>
		"\x00" { return errno = EILSEQ, END; }

	<doc, param, math, code, macro> whitespace+ { goto reset; }
	<code, macro> cxx_comment { goto reset; }

	<doc, param, math, code, comment, macro_comment> newline {
		scanner.line++;
		if(scanner.state == yycdoc) return DOC_NEWLINE;
		goto reset;
	}
	<string, character> newline {
		return errno = EILSEQ, END;
	}
	<macro> newline {
		scanner.line++;
		scanner.state = yyccode;
		goto reset;
	}
	// Continuation.
	<string, character, macro> "\\" newline { scanner.line++; goto scan; }
	<doc> art / [^/] {
		scanner.line++;
		if(scanner.state == yycdoc) return DOC_NEWLINE;
		goto reset;
	}

	<doc> "*""/" { return scanner.state = yyccode, DOC_END; }
	<param, math> "*""/" { fprintf(stderr, "Not finished at end of comment.\n");
		return errno = EILSEQ, END; }

	<doc> (wordchars | ",")* { return DOC_WORD; }
	<param> (wordchars | "_" | "`" | "\\")* { return PARAM_ITEM; }
	<param> "," { return PARAM_COMMA; }
	<math> (wordchars | "," | "_")* { return MATH_STUFF; }

	<doc, param, math> "\\\\"      { return DOC_BACKSLASH; }
	<doc, param, math> "\\`"       { return DOC_BACKQUOTE; }
	<doc, param, math> "\\@" | "@" { return DOC_EACH; }
	<doc, param, math> "\\_"       { return DOC_UNDERSCORE; }
	<doc> "_"                      { return DOC_ITALICS; }
	<doc> "`" { return scanner.state = yycmath, MATH_BEGIN; }
	<math> "`" { return scanner.state = yycdoc, MATH_END; }
	<doc> "{" { return scanner.state = yycparam, PARAM_BEGIN; }
	<doc> "}" { fprintf(stderr, "Misplaced '}' in documentation.\n");
		return errno = EILSEQ, END; }
	<param> "{" { fprintf(stderr, "Misplaced '{' in documentation.\n");
		return errno = EILSEQ, END; }
	<param> "}" :=> doc

	// These are recognised in the documentation as stuff.
	<doc> "\\url"  { return DOC_URL; }
	<doc> "\\cite" { return DOC_CITE; }
	<doc> "\\see"  { return DOC_SEE; }

	// These are tags.
	<doc> "@title"      { return TAG_TITLE; }
	<doc> "@param"      { return TAG_PARAM; }
	<doc> "@author"     { return TAG_AUTHOR; }
	<doc> "@std"        { return TAG_STD; }
	<doc> "@depend"     { return TAG_DEPEND; }
	<doc> "@version"    { return TAG_VERSION; }
	<doc> "@since"      { return TAG_SINCE; }
	<doc> "@fixme"      { return TAG_FIXME; }
	<doc> "@deprecated" { return TAG_DEPRICATED; }
	<doc> "@return"     { return TAG_RETURN; }
	<doc> "@throws"     { return TAG_THROWS; }
	<doc> "@implements" { return TAG_IMPLEMENTS; }
	<doc> "@order"      { return TAG_ORDER; }
	<doc> "@allow"      { return TAG_ALLOW; }

	// Also escape these for {HTML}.
	<doc, param, math> "&" { return DOC_HTML_AMP; }
	<doc, param, math> "<" { return DOC_HTML_LT; }
	<doc, param, math> ">" { return DOC_HTML_GT; }

	<code> doc / [^/] :=> doc
	// With flattening the `ScanState` stack, this is not actually worth
	// the effort; honestly, it's not going to matter.
	<macro> doc / [^/] {
		fprintf(stderr, "Documentation comment inside macro.\n");
		debug();
		return errno = EILSEQ, END;
	}
	<code> begin_comment :=> comment
	<macro> begin_comment :=> macro_comment
	<code> macro :=> macro
	<code> "L"? "\"" :=> string
	<code> "'" :=> character
	<code> number       { return C_CONSTANT; }
	<code> operator     { return C_OPERATOR; }
	<code> generic      { return C_ID_ONE_GENERIC; }
	<code> generic generic { return C_ID_TWO_GENERICS; }
	<code> generic generic generic { return C_ID_THREE_GENERICS; }
	<code> "struct"     { return C_STRUCT; }
	<code> "union"      { return C_UNION; }
	<code> "enum"       { return C_ENUM; }
	<code> "typedef"    { return C_TYPEDEF; }
	<code> "static"     { return C_STATIC; }
	<code> "void"       { return C_VOID; }
	<code> ("{" | "<%") { scanner.indent_level++; return C_LBRACE; }
	<code> ("}" | "%>") { scanner.indent_level--; return C_RBRACE; }
	<code> ("[" | "<:") { return C_LBRACK; }
	<code> ("]" | ":>") { return C_RBRACK; }
	<code> "("          { return C_LPAREN; }
	<code> ")"          { return C_RPAREN; }
	<code> ","          { return C_COMMA; }
	<code> ";"          { return C_SEMI; }
	<code> id           { return C_ID; }

	<comment> end_comment :=> code
	<macro_comment> end_comment :=> macro

	<string> "\""   { scanner.state = yyccode; return C_CONSTANT; }
	<character> "'" { scanner.state = yyccode; return C_CONSTANT; }
	// All additional chars are not escaped.
	<string, character> "\\". { goto scan; }
*/
}

int main(int argc, char **argv) {

	/* https://stackoverflow.com/questions/10293387/piping-into-application-run-under-xcode/13658537 */
	if (argc == 2 && strcmp(argv[1], "debug") == 0 ) {
		const char *test_file_path = "/Users/neil/Movies/Cdoc/c.txt";
		printf("== [RUNNING IN DEBUG MODE with %s]==\n\n", test_file_path);
		freopen(test_file_path, "r", stdin);
	}

	if(!Scanner()) goto catch;
	while((scanner.symbol = scan())) append(scanner.symbol), debug();
	if(errno) goto catch;
	{
		struct Segment *segment = 0;
		fputs("\n -- Print out: --\n", stdout);
		while((segment = SegmentArrayNext(&doc, segment))) {
			struct Tag *tag = 0;
			printf("Segment(%s):\n\tdoc: %s.\n\tcode: %s.\n",
				sections[segment->section], TokenArrayToString(&segment->doc),
				TokenArrayToString(&segment->code));
			while((tag = TagArrayNext(&segment->tags, tag))) {
				printf("\t%s{%s} %s.\n", symbols[tag->token.symbol],
					TokenArrayToString(&tag->header),
					TokenArrayToString(&tag->contents));
			}
		}
		fputc('\n', stdout);
	}
	doc_();
	Scanner_();
	return EXIT_SUCCESS;

catch:
	return perror("scanner"), doc_(), Scanner_(), EXIT_FAILURE;
}
