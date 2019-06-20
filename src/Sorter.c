/** 2019 Neil Edelman, distributed under the terms of the MIT License,
 \url{ https://opensource.org/licenses/MIT }.

 {Doxygen} should be used if possible. However, maybe one is too hip to use it?
 (or it uses libraries that are too new for one's computer.) The documentation
 is a perversion and simplification of {Doxygen}.

 Parses and extracts the documentation commands in a {.c} file. A documentation
 command begins with {/\**}.

 Sorts and stores the parsed file into sections.

 @title Cdoc.c
 @author Neil
 @version 2019-05 Re-done in {re2c}.
 @since 2017-03 Initial version.
 @std C89
 @depend re2c (\url{http://re2c.org/})
 @fixme Different doc comments need new paragraphs.
 @fixme Lists in comments, etc.
 @fixme {void A_BI_(Create, Thing)(void)} -> {<A>Create<BI>Thing(void)}.
 @fixme Trigraph support. */

#include <stdlib.h> /* EXIT malloc free */
#include <stdio.h>  /* FILE printf fputc perror */
#include <string.h> /* memmove memset */
#include <errno.h>  /* EDOM ERANGE */
/* #define NDEBUG */
#include <assert.h> /* assert */

#include "Scanner.h"
#include "Sorter.h"
#include "Marker.h"

/* Private symbol infomation. From `Scanner.h`. */
enum SymbolOutput { SYMBOL_OUTPUT(PARAM) };
static const int symbol_output[] = { SYMBOL(PARAM3_B) };
static const char *const symbol_outputs[] = { SYMBOL_OUTPUT(STRINGISE) };

/* Define the sections of output. */
#define SECTION(X) X(UNDECIDED), X(HEADER), X(DECLARATION), X(FUNCTION)
enum Section { SECTION(PARAM) };
static const char *const sections[] = { SECTION(STRINGISE) };



/* `Token` is in `Scanner.h` to be used by `ScannerToken`. */

static void token_to_string(const struct Token *s, char (*const a)[12]) {
	int len = s->length >= 5 ? 5 : s->length;
	sprintf(*a, "%.4s<%.*s>", symbols[s->symbol], len, s->from);
	/*strncpy(*a, symbols[s->symbol], sizeof *a - 1);*/
	*a[sizeof *a - 1] = '\0';/*???*/
}

#define ARRAY_NAME Token
#define ARRAY_TYPE struct Token
#define ARRAY_TO_STRING &token_to_string
#include "../src/Array.h"

/** `TokenArraySize` is used in `Marker`. */
size_t TokensSize(const struct TokenArray *const ta) {
	return TokenArraySize(ta);
}

/** `TokenArrayNext` is used in `Marker`. */
struct Token *TokensNext(const struct TokenArray *const a,
	struct Token *const here) {
	return TokenArrayNext(a, here);
}

/** `token.symbol` is used in `Marker`. */
enum Symbol TokenSymbol(const struct Token *const token) {
	assert(token);
	return token->symbol;
}

/* `static` constants that require memory so are initialised on start and
 deleted on end. */
static struct TokenArray paragraph;



/** Tags are documentation symbols and everything that comes after.
 Ie, `tag = TAG_TITLE, header = {}, contents = { all, , your, , base }` or
 `tag = TAG_PARAM, header = { ID"x" }, contents = { the, , dependant, ,
 varible }`. */
struct Tag {
	struct Token token;
	struct TokenArray header;
	struct TokenArray contents;
};

static void tag_to_string(const struct Tag *t, char (*const a)[12]) {
	strncpy(*a, symbols[t->token.symbol], sizeof *a - 1),
		*a[sizeof *a - 1] = '\0';
}

static void tag_init(struct Tag *const tag) {
	assert(tag);
	TokenArray(&tag->header);
	TokenArray(&tag->contents);
	ScannerToken(&tag->token);
}

#define ARRAY_NAME Tag
#define ARRAY_TYPE struct Tag
#define ARRAY_TO_STRING &tag_to_string
#include "Array.h"

static void tag_array_remove(struct TagArray *const ta) {
	struct Tag *t;
	if(!ta) return;
	while((t = TagArrayPop(ta)))
		TokenArray_(&t->header), TokenArray_(&t->contents);
	TagArray_(ta);
}



/** A {SegmentArray} is the top level parser. `Segment` is classified to a
 section of the document and can have documentation including tags and code.
 Ie, `section = FUNCTION, doc = { I, , don't, , know, , what, , this, , fn, ,
 does }`, `code = { int foo(int foo) {} }`, `tags = { { TAG_PARAM, { does, ,
 nothing } }, { TAG_ALLOW, { } } }`. */
struct Segment {
	enum Section section;
	struct TokenArray doc, code;
	struct TagArray tags;
};

static void segment_init(struct Segment *const segment) {
	assert(segment);
	segment->section = UNDECIDED; /* Default. */
	TokenArray(&segment->doc);
	TokenArray(&segment->code);
	TagArray(&segment->tags);
}

static void segment_to_string(const struct Segment *seg, char (*const a)[12]) {
	strncpy(*a, sections[seg->section], sizeof *a - 1),
		*a[sizeof *a - 1] = '\0';
}

#define ARRAY_NAME Segment
#define ARRAY_TYPE struct Segment
#define ARRAY_TO_STRING &segment_to_string
#include "../src/Array.h"

static void segment_array_remove(struct SegmentArray *const sa) {
	struct Segment *s;
	if(!sa) return;
	while((s = SegmentArrayPop(sa)))
		TokenArray_(&s->doc), TokenArray_(&s->code),
		tag_array_remove(&s->tags);
	SegmentArray_(sa);
}



/** Cleans the whitespace so it's just in between words and adds paragraphs
 where needed. */
static void clean_whitespace(struct TokenArray *const sa) {
	const struct TokenArray *replace;
	struct Token *x = 0, *x_start = 0;
	size_t count_nl = 0;
	int is_content = 0;
	assert(sa);
	while((x = TokenArrayNext(sa, x))) {
		if(x->symbol == NEWLINE) {
			if(!x_start) x_start = x;
			count_nl++;
		} else {
			if(x_start) {
				replace = (is_content && count_nl > 1) ? &paragraph : 0;
				count_nl = 0;
				TokenArrayReplace(sa, x_start, (long)(x - x_start), replace);
				x = x_start + TokenArraySize(replace);
				x_start = 0;
			}
			is_content = 1;
		}
	}
	/* Whitespace at end of section. */
	if(x_start) TokenArrayReplace(sa, x_start, -1, 0);
	printf("Parser:Clean: %s.\n", TokenArrayToString(sa));
}

/** @implements{Predicate<Segment>} */
static int keep_segment(const struct Segment *const s) {
	if(TokenArraySize(&s->doc) || s->section == FUNCTION) return 1;
	return 0;
}

static struct Sorter {
	int is_matching, is_indent, is_struct, is_differed_cut;
	struct Token token;
	struct TokenInfo info;
	struct Tag *tag;
	struct TokenArray *chosen;
} sorter;

static void sorter_end_segment(void) {
	sorter.is_differed_cut = 0, sorter.is_struct = 0, sorter.tag = 0;
}

static void sorter_err(void) {
	fprintf(stderr, "At %lu%c indent level %d; state stack %s; %s \"%.*s\".\n",
		(unsigned long)sorter.token.line, sorter.info.is_doc ? '~' : ':',
		sorter.info.indent_level, ScannerStates(),
		symbols[sorter.token.symbol], sorter.token.length, sorter.token.from);
}

int main(int argc, char **argv) {
	struct SegmentArray segments = ARRAY_ZERO;
	struct Segment *segment = 0;
	int is_done = 0;

	/* https://stackoverflow.com/questions/10293387/piping-into-application-run-under-xcode/13658537 */
	if (argc == 2 && strcmp(argv[1], "debug") == 0 ) {
		const char *test_file_path = "/Users/neil/Movies/Cdoc/c.txt";
		printf("== [RUNNING IN DEBUG MODE with %s]==\n\n", test_file_path);
		freopen(test_file_path, "r", stdin);
	}

	{ /* Constant dynamically allocated things. */
		struct Token *t;
		if(!(t = TokenArrayNew(&paragraph))) goto catch;
		t->symbol = NEWLINE;
		t->from = 0, t->length = 0, t->line = 0;
	}

	/* Lex. */
	if(!Scanner()) goto catch;
	while((ScannerNext())) {
		ScannerToken(&sorter.token);
		ScannerTokenInfo(&sorter.info);
		sorter.is_matching = !sorter.info.indent_level;
		{ /* Print debug. */
			int indent;
			printf("%lu%c\t", (unsigned long)sorter.token.line,
				sorter.info.is_doc ? '~' : ':');
			for(indent = 0; indent < sorter.info.indent_level; indent++)
				fputc('\t', stdout);
			printf("%s %s \"%.*s\"\n", ScannerStates(),
				symbols[sorter.token.symbol], sorter.token.length,
				sorter.token.from);
		}
		if(!sorter.is_indent) { /* Global scope. */
#if 1
			if(sorter.info.indent_level) { /* Entering a block. */
				assert(sorter.info.indent_level == 1 && !sorter.info.is_doc
					&& sorter.token.symbol == LBRACE);
				sorter.is_indent = 1;
			} else if(sorter.token.symbol == SEMI) { /* Global semicolons cut after. */
				sorter.is_differed_cut = 1;
			} else if(segment && !TokenArraySize(&segment->code)
				&& (sorter.token.symbol == BEGIN_DOC
				|| (!sorter.info.is_doc && sorter.info.is_doc_far))) {
				/* fixme: wtf did I write there? */
				/* Hasn't scanned any code and is on the top level, cut
				 multiple docs and the doc has to be within a reasonable
				 distance. */
				printf("<cut>\n"), sorter_end_segment();
			}
#endif
		} else { /* In code block. */
			if(!sorter.info.indent_level) { /* Exiting to global scope. */
#if 1
				assert(!sorter.info.is_doc && sorter.token.symbol == RBRACE);
				sorter.is_indent = 0;
				if(!sorter.is_struct) sorter.is_differed_cut = 1; /* Functions. */
#endif
			} else if(!sorter.is_struct && !sorter.info.is_doc) {
				continue; /* Code in functions: don't care. */
			}
		}
		/* This is a symbol that is for splitting up multiple doc
		 comments on a single line -- ignore. */
		if(sorter.token.symbol == BEGIN_DOC) continue;
		/* Create new segment if need be. */
		if(!segment) {
			printf("<new segment>\n");
			if(!(segment = SegmentArrayNew(&segments)))
				{ sorter_err(); goto catch; }
			segment_init(segment);
		}
		/* Choose the token array. */
		if(sorter.info.is_doc) {
			/*if(symbol_type[token.symbol] == TYPE_TAG) {
				printf("---> should been new %s\n", symbols[token.symbol]);
				if(!(tag = TagArrayNew(&segment->tags)))
					{ err_info(&token, &info); goto catch; }
				tag_init(tag);
				tokens = &tag->contents;
				continue;
			} else {
				tokens = &segment->doc;
			}*/
			sorter.chosen = &segment->doc;
		} else {
			sorter.chosen = &segment->code;
		}
		{
			struct Token *token;
			/* Push symbol. */
			if(!(token = TokenArrayNew(sorter.chosen)))
				{ sorter_err(); goto catch; }
			ScannerToken(token);
		}
		/* Create another segment next time. */
		sorter_end_segment();
	}

	if(!sorter.is_matching) { fprintf(stderr, "Braces do not match at EOF.\n");
		errno = EILSEQ; goto catch; }

	/* Cull. Rid uncommented blocks. Whitespace clean-up, (after!) */
	SegmentArrayKeepIf(&segments, &keep_segment);
	segment = 0;
	while((segment = SegmentArrayNext(&segments, segment)))
		clean_whitespace(&segment->doc);

	segment = 0;
	fputs("\n\n*****\n\n", stdout);
	while((segment = SegmentArrayNext(&segments, segment))) {
		struct Tag *tag;
		printf("Segment(%s):\n\tdoc: %s.\n\tcode: %s.\n",
			sections[segment->section], TokenArrayToString(&segment->doc),
			TokenArrayToString(&segment->code));
		tag = 0;
		while((tag = TagArrayNext(&segment->tags, tag))) {
			printf("\t%s.\n", symbols[tag->token.symbol]);
		}
	}
	fputc('\n', stdout);

	is_done = 1;
	goto finally;

catch:
	perror("Cdoc");

finally:
	Marker(0);
	segment_array_remove(&segments);
	Scanner_();
	TokenArray_(&paragraph);

	return is_done ? EXIT_SUCCESS : EXIT_FAILURE;
}
