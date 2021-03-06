/** @license 2019 Neil Edelman, distributed under the terms of the
 [MIT License](https://opensource.org/licenses/MIT).

 Organises tokens into sections, each section can have some documentation,
 code, and maybe attributes. */

#include <string.h> /* size_t strncpy strncmp */
#include <limits.h> /* INT_MAX */
#include <stdio.h>  /* .printf */
#include "Division.h"
#include "Format.h"
#include "Scanner.h"
#include "Semantic.h"
#include "UrlEncode.h"
#include "Buffer.h"
#include "Path.h"
#include "Text.h"
#include "Style.h"
#include "ImageDimension.h"
#include "Cdoc.h"
#include "Report.h"


/** `Token` has a `Symbol` and is associated with an area of the text. */
struct Token {
	enum Symbol symbol;
	const char *from;
	int length;
	const char *label;
	size_t line;
};
static void token_to_string(const struct Token *t, char (*const a)[12]) {
	switch(t->symbol) {
	case WORD: { int len = t->length >= 9 ? 9 : t->length;
		sprintf(*a, "<%.*s>", len, t->from); break; }
	case DOC_ID:
	case ID: { int len = t->length >= 8 ? 8 : t->length;
		sprintf(*a, "ID:%.*s", len, t->from); break; }
	case SPACE: { (*a)[0] = '~', (*a)[1] = '\0'; break; }
	default:
		strncpy(*a, symbols[t->symbol], sizeof *a - 1);
		(*a)[sizeof *a - 1] = '\0';
	}
}
/** Compares the _contents_ of the tokens. */
static int token_compare(const struct Token *const a,
	const struct Token *const b) {
	const int len_cmp = (a->length > b->length) - (b->length > a->length);
	const int str_cmp = strncmp(a->from, b->from,
		len_cmp >= 0 ? b->length : a->length);
	return str_cmp ? str_cmp : len_cmp;
}
#define ARRAY_NAME Token
#define ARRAY_TYPE struct Token
#define ARRAY_TO_STRING &token_to_string
#include "Array.h"
/** This is used in `Semantic.c.re` to get the first file:line for error. */
const char *TokensFirstLabel(const struct TokenArray *const tokens) {
	const struct Token *const first = TokenArrayNext(tokens, 0);
	return first ? first->label : "unlabelled";
}
size_t TokensFirstLine(const struct TokenArray *const tokens) {
	const struct Token *const first = TokenArrayNext(tokens, 0);
	return first ? first->line : 0;
}
/** This is used in `Semantic.c.re` to get the size of the string for
 `tokens`. */
size_t TokensMarkSize(const struct TokenArray *const tokens) {
	if(!tokens) return 0;
	return TokenArraySize(tokens) + 1;
}
/** @param[tokens] The `TokenArray` that converts to a string.
 @param[marks] Must be an at-least `TokensMarkSize` buffer, or null.
 @return The size of the string, including null. */
void TokensMark(const struct TokenArray *const tokens, char *const marks) {
	struct Token *token = 0;
	char *mark = marks;
	if(!marks) return;
	while((token = TokenArrayNext(tokens, token)))
		*mark++ = symbol_marks[token->symbol];
	*mark = '\0';
	assert((size_t)(mark - marks) == TokenArraySize(tokens));
}


static void index_to_string(const size_t *const n, char (*const a)[12]) {
	sprintf(*a, "%lu", (unsigned long)*n % 1000000000lu);
}
#define ARRAY_NAME Index
#define ARRAY_TYPE size_t
#define ARRAY_TO_STRING &index_to_string
#include "Array.h"


/** `Attribute` is a specific structure of array of `Token` representing
 each-attributes, "\@param ...". */
struct Attribute {
	struct Token token;
	struct TokenArray header;
	struct TokenArray contents;
};
static void attribute_to_string(const struct Attribute *t, char (*const a)[12])
{
	strncpy(*a, symbols[t->token.symbol], sizeof *a - 1);
	(*a)[sizeof *a - 1] = '\0';
}
#define ARRAY_NAME Attribute
#define ARRAY_TYPE struct Attribute
#define ARRAY_TO_STRING &attribute_to_string
#include "../src/Array.h"
static void attributes_(struct AttributeArray *const atts) {
	struct Attribute *a;
	if(!atts) return;
	while((a = AttributeArrayPop(atts)))
		TokenArray_(&a->header), TokenArray_(&a->contents);
	AttributeArray_(atts);
}


/** `Segment` is classified to a section of the document and can have
 documentation including attributes and code. */
struct Segment {
	enum Division division;
	struct TokenArray doc, code;
	struct IndexArray code_params;
	struct AttributeArray attributes;
};
/** Provides a default token for `segment` to print. */
static const struct Token *segment_fallback(const struct Segment *const segment,
	const struct TokenArray **const ta_ptr) {
	const struct TokenArray *ta = 0;
	const struct Token *t = 0;
	assert(segment);
	if(IndexArraySize(&segment->code_params)) {
		const size_t i = IndexArrayGet(&segment->code_params)[0];
		ta = &segment->code;
		assert(i < TokenArraySize(ta));
		t = TokenArrayGet(ta) + i;
	} else if(TokenArraySize(&segment->code)) {
		ta = &segment->code;
		t = TokenArrayGet(ta);
	} else if(!ta_ptr && TokenArraySize(&segment->doc)) {
		/* /\ Raw pointers in the text are problematic since maybe we will
		 convert it to a string and most text does not support that. */
		ta = &segment->doc;
		t = TokenArrayGet(ta);
	} else if(!ta_ptr && AttributeArraySize(&segment->attributes)) {
		ta = &segment->doc;
		t = &AttributeArrayGet(&segment->attributes)->token;
	}
	if(ta_ptr) *ta_ptr = ta;
	return t;
	/*return IndexArraySize(&segment->code_params)
		? TokenArrayGet(&segment->code)
		+ IndexArrayGet(&segment->code_params)[0]
		: TokenArraySize(&segment->code) ? TokenArrayGet(&segment->code)
		: TokenArraySize(&segment->doc) ? TokenArrayGet(&segment->doc)
		: AttributeArraySize(&segment->attributes)
		? &AttributeArrayGet(&segment->attributes)->token : 0;*/
}

/* For <fn:segment_to_string>. */
static const char *print_token_s(const struct TokenArray *const tokens,
	const struct Token *token);

static void segment_to_string(const struct Segment *segment,
	char (*const a)[12]) {
	const struct TokenArray *ta;
	const struct Token *const fallback = segment_fallback(segment, &ta);
	const char *temp = divisions[segment->division];
	size_t temp_len, i = 0;
	if(fallback) {
		StylePush(ST_TO_RAW);
		temp = print_token_s(ta, fallback);
		StylePop();
	}
	temp_len = strlen(temp);
	if(temp_len > sizeof *a - 3) temp_len = sizeof *a - 3;
	(*a)[i++] = 'S';
	(*a)[i++] = '_';
	memcpy(*a + i, temp, temp_len);
	i += temp_len;
	(*a)[i++] = '\0';
	assert(i <= sizeof *a);
}
static void erase_segment(struct Segment *const segment) {
	char a[12];
	assert(segment);
	segment_to_string(segment, &a);
	segment->division = DIV_PREAMBLE;
	TokenArray_(&segment->doc);
	TokenArray_(&segment->code);
	if(CdocGetDebug() & DBG_ERASE && IndexArraySize(&segment->code_params))
		fprintf(stderr, "*** Erasing %s: %s.\n",
		a, IndexArrayToString(&segment->code_params));
	IndexArray_(&segment->code_params);
	attributes_(&segment->attributes);
}
#define ARRAY_NAME Segment
#define ARRAY_TYPE struct Segment
#define ARRAY_TO_STRING &segment_to_string
#include "../src/Array.h"
static void segment_array_(struct SegmentArray *const sa) {
	struct Segment *segment;
	if(!sa) return;
	while((segment = SegmentArrayPop(sa))) erase_segment(segment);
	SegmentArray_(sa);
}
/*static void segment_array_clear(struct SegmentArray *const sa) {
	struct Segment *segment;
	if(!sa) return;
	while((segment = SegmentArrayPop(sa)))
		TokenArrayClear(&segment->doc), TokenArrayClear(&segment->code),
		IndexArrayClear(&segment->code_params),
		attributes_(&segment->attributes);
}*/
static const struct Token *param_no(const struct Segment *const segment,
	const size_t param) {
	size_t *pidx;
	assert(segment);
	if(param >= IndexArraySize(&segment->code_params)) return 0;
	pidx = IndexArrayGet(&segment->code_params) + param;
	/* This is really careful. */
	if(*pidx >= TokenArraySize(&segment->code)) {
		char a[12];
		segment_to_string(segment, &a);
		fprintf(stderr, "%s: param index %lu is greater then code size.\n",
			a, (unsigned long)TokenArraySize(&segment->code));
		return 0;
	}
	return TokenArrayGet(&segment->code) + *pidx;
}



/** Top-level static document. */
static struct SegmentArray report;
static struct TokenArray brief;



/** Destructor for the static document. Also destucts the string used for
 tokens. */
void Report_(void) {
	TokenArray_(&brief);
	segment_array_(&report);
	Semantic(0);
	Style_();
}

/** @return A new empty segment from `segments`, defaults to the preamble, or
 null on error. */
static struct Segment *new_segment(struct SegmentArray *const segments) {
	struct Segment *segment;
	assert(segments);
	if(!(segment = SegmentArrayNew(segments))) return 0;
	segment->division = DIV_PREAMBLE; /* Default. */
	TokenArray(&segment->doc);
	TokenArray(&segment->code);
	IndexArray(&segment->code_params);
	AttributeArray(&segment->attributes);
	return segment;
}

/** Initialises `token` with `scan`.
 @return Success.
 @throws[EILSEQ] The token cannot be represented as an `int` offset. */
static int init_token(struct Token *const token,
	const struct Scanner *const scan) {
	const char *const from = ScannerFrom(scan), *const to = ScannerTo(scan);
	assert(scan && token && from && from <= to);
	if(from + INT_MAX < to) return errno = EILSEQ, 0;
	token->symbol = ScannerSymbol(scan);
	token->from = from;
	token->length = (int)(to - from);
	token->label = ScannerLabel(scan);
	token->line = ScannerLine(scan);
	return 1;
}

/** @return A new `Attribute` on `segment` with `symbol`, (should be a
 attribute symbol.) Null on error. */
static struct Attribute *new_attribute(struct Segment *const segment,
	const struct Scanner *const scan) {
	struct Attribute *att;
	assert(scan && segment);
	if(!(att = AttributeArrayNew(&segment->attributes))) return 0;
	init_token(&att->token, scan);
	TokenArray(&att->header);
	TokenArray(&att->contents);
	return att;
}

/** Creates a new token from `tokens` and fills it with `symbol` and the
 most recent scanner location.
 @return Token or failure. */
static struct Token *new_token(struct TokenArray *const tokens,
	const struct Scanner *const scan) {
	struct Token *token;
	if(!(token = TokenArrayNew(tokens))) return 0;
	init_token(token, scan);
	/*fprintf(stderr, "new_token: %s %.*s\n", symbols[token->symbol],
		token->length, token->from); <- If one really wants spam. */
	return token;
}

/** Wrapper for `Semantic.h`; extracts semantic information from `segment`. */
static int report_semantic(struct Segment *const segment) {
	size_t no, i;
	const size_t *source;
	size_t *dest;
	if(!segment) return 0;
	if(!Semantic(&segment->code)) return 0;
	segment->division = SemanticDivision();
	/* Copy `Semantic` size array to this size array,
	 (not the same, local scope; kind of a hack.) */
	SemanticParams(&no, &source);
	if(!no) return 1; /* We will cull them later. */
	if(!(dest = IndexArrayBuffer(&segment->code_params, no))) return 0;
	for(i = 0; i < no; i++) dest[i] = source[i];
	if(CdocGetDebug() & DBG_ERASE) {
		char a[12];
		segment_to_string(segment, &a);
		fprintf(stderr, "*** Adding %lu to %s: %s.\n",
			(unsigned long)no, a, IndexArrayToString(&segment->code_params));
	}
	return 1;
}



/** Prints `segment` to `stderr`. */
static void print_segment_debug(const struct Segment *const segment) {
	struct Attribute *att = 0;
	struct Token *doc, *code;
	if(!(CdocGetDebug() & DBG_OUTPUT)) return;
	code = TokenArrayNext(&segment->code, 0);
	doc  = TokenArrayNext(&segment->doc,  0);
	fprintf(stderr, "Segment division %s:\n"
		"%s:%lu code: %s;\n"
		"of which params: %s;\n"
		"%s:%lu doc: %s.\n",
		divisions[segment->division],
		code ? code->label : "N/A", code ? code->line : 0,
		TokenArrayToString(&segment->code),
		IndexArrayToString(&segment->code_params),
		doc ? doc->label : "N/A", doc ? doc->line : 0,
		TokenArrayToString(&segment->doc));
	while((att = AttributeArrayNext(&segment->attributes, att)))
		fprintf(stderr, "%s{%s} %s.\n", symbols[att->token.symbol],
		TokenArrayToString(&att->header),
		TokenArrayToString(&att->contents));
}

/** Helper for next. Note that if it stops on a preamble command, this is not
 printed. */
static void cut_segment_here(struct Segment **const psegment) {
	const struct Segment *segment = 0;
	assert(psegment);
	if(!(segment = *psegment)) return;
	print_segment_debug(segment);
	*psegment = 0;
}

/** Prints line info into a static buffer. */
static const char *oops(const struct Scanner *const scan) {
	static char p[128];
	assert(scan);
	sprintf(p, "%.32s:%lu, %s", ScannerLabel(scan),
		(unsigned long)ScannerLine(scan), symbols[ScannerSymbol(scan)]);
	return p;
}

void ReportLastSegmentDebug(void) {
	const struct Segment *const segment = SegmentArrayBack(&report, 0);
	if(!segment) return;
	print_segment_debug(segment);
}

/** This appends the current token based on the state it was last in.
 @return Success. */
int ReportNotify(const struct Scanner *const scan) {
	const enum Symbol symbol = ScannerSymbol(scan);
	const char symbol_mark = symbol_marks[symbol];
	int is_differed_cut = 0;
	static struct {
		enum { S_CODE, S_DOC, S_ARGS } state;
		size_t last_doc_line;
		struct Segment *segment;
		struct Attribute *attribute;
		unsigned space, newline;
		int is_code_ignored, is_semantic_set;
	} sorter = { 0, 0, 0, 0, 0, 0, 0, 0 };
	/* These symbols require special consideration. */
	switch(symbol) {
	case DOC_BEGIN:
		if(sorter.state != S_CODE) return fprintf(stderr,
			"%s: sneak path; was expecting code.\n",
			oops(scan)), errno = EDOM, 0;
		sorter.state = S_DOC;
		/* Reset attribute. */
		sorter.attribute = 0;
		/* Two docs on top of each other without code, the top one belongs to
		 the preamble. */
		if(sorter.segment && !TokenArraySize(&sorter.segment->code))
			cut_segment_here(&sorter.segment);
		return 1;
	case DOC_END:
		if(sorter.state != S_DOC) return fprintf(stderr,
			"%s: sneak path; was expecting doc.\n",
			oops(scan)), errno = EDOM, 0;
		sorter.state = S_CODE;
		sorter.last_doc_line = ScannerLine(scan);
		return 1;
	case DOC_LEFT:
		if(sorter.state != S_DOC || !sorter.segment || !sorter.attribute)
			return fprintf(stderr,
			"%s: sneak path; was expecting doc with attribute.\n", oops(scan)),
			errno = EDOM, 0;
		sorter.state = S_ARGS;
		return 1;
	case DOC_RIGHT:
		if(sorter.state != S_ARGS || !sorter.segment || !sorter.attribute)
			return fprintf(stderr,
			"%s: sneak path; was expecting args with attribute.\n", oops(scan)),
			errno = EDOM, 0;
		sorter.state = S_DOC;
		return 1;
	case DOC_COMMA: /* @arg[,,] */
		if(sorter.state != S_ARGS || !sorter.segment || !sorter.attribute)
			return fprintf(stderr,
			"%s: sneak path; was expecting args with attribute.\n", oops(scan)),
			errno = EDOM, 0;
		return 1;
	case SPACE:   sorter.space++; return 1;
	case NEWLINE: sorter.newline++; return 1;
	case SEMI:
		/* Break on global semicolons only. */
		if(ScannerIndentLevel(scan) != 0 || !sorter.segment) break;
		/* Find out what this line means if one hasn't already. */
		if(!sorter.is_semantic_set && !report_semantic(sorter.segment)) return 0;
		sorter.is_semantic_set = 1;
		is_differed_cut = 1;
		break;
	case LBRACE:
		/* If it's a leading brace, see what the Semantic says about it. */
		if(ScannerIndentLevel(scan) != 1 || sorter.is_semantic_set
			|| !sorter.segment) break;
		if(!report_semantic(sorter.segment)) return 0;
		sorter.is_semantic_set = 1;
		if(sorter.segment->division == DIV_FUNCTION) sorter.is_code_ignored = 1;
		break;
	case RBRACE:
		/* Functions don't have ';' to end them. */
		if(ScannerIndentLevel(scan) != 0 || !sorter.segment) break;
		if(sorter.segment->division == DIV_FUNCTION) is_differed_cut = 1;
		break;
	case LOCAL_INCLUDE: /* Include file. */
		assert(sorter.state == S_CODE);
		{
			const char *fn = 0;
			struct Scanner *subscan = 0;
			struct Text *text = 0;
			int success = 0;
			if(!(fn = PathFromHere(ScannerTo(scan) - ScannerFrom(scan),
				ScannerFrom(scan)))) goto include_catch;
			if(!(text = TextOpen(fn))) goto include_catch;
			cut_segment_here(&sorter.segment);
			if(!(subscan = Scanner(TextBaseName(text), TextGet(text),
				&ReportNotify, SSCODE))) goto include_catch;
			cut_segment_here(&sorter.segment);
			success = 1;
			goto include_finally;
include_catch:
			if(errno) perror("including");
			else fprintf(stderr, "%s: couldn't resolve name.\n", oops(scan));
include_finally:
			Scanner_(&subscan);
			return success;
		}
	default: break;
	}

	/* Code that starts far away from docs goes in it's own segment. */
	if(sorter.segment && symbol_mark != '~' && symbol_mark != '@'
		&& !TokenArraySize(&sorter.segment->code) && sorter.last_doc_line
		&& sorter.last_doc_line + 2 < ScannerLine(scan))
		cut_segment_here(&sorter.segment);

	/* Make a new segment if needed. */
	if(!sorter.segment) {
		if(!(sorter.segment = new_segment(&report))) return 0;
		sorter.attribute = 0;
		sorter.space = sorter.newline = 0;
		sorter.is_code_ignored = sorter.is_semantic_set = 0;
	}

	/* Make a `token` where the context places us. */
	switch(symbol_mark) {
	case '~': /* General docs. */
		assert(sorter.state == S_DOC || sorter.state == S_ARGS);
		{ /* This lazily places whitespace and newlines. */
			struct TokenArray *selected = sorter.attribute
				? (sorter.state == S_ARGS ? &sorter.attribute->header
				: &sorter.attribute->contents) : &sorter.segment->doc;
			struct Token *tok;
			const int is_para = sorter.newline > 1,
				is_space = sorter.space || sorter.newline,
				is_doc_empty = !TokenArraySize(&sorter.segment->doc),
				is_selected_empty = !TokenArraySize(selected);
			sorter.space = sorter.newline = 0;
			if(is_para) {
				/* Switch out of attribute when on new paragraph. */
				sorter.attribute = 0, sorter.state = S_DOC;
				selected = &sorter.segment->doc;
				if(!is_doc_empty) {
					if(!(tok = new_token(selected, scan))) return 0;
					tok->symbol = NEWLINE; /* Override whatever's there. */
				}
			} else if(is_space && !is_selected_empty) {
				if(!(tok = new_token(selected, scan))) return 0;
				tok->symbol = SPACE; /* Override. */
			}
			if(!new_token(selected, scan)) return 0;
		}
		break;
	case '@': /* An attribute marker. */
		assert(sorter.state == S_DOC);
		if(!(sorter.attribute = new_attribute(sorter.segment, scan)))
			return 0;
		sorter.space = sorter.newline = 0; /* Also reset this for attributes. */
		break;
	default: /* Code. */
		assert(sorter.state == S_CODE);
		if(sorter.is_code_ignored) break;
		if(!new_token(&sorter.segment->code, scan)) return 0;
		break;
	}

	/* End the segment. */
	if(is_differed_cut) cut_segment_here(&sorter.segment);

	return 1;
}

/** Used for temporary things in doc mode.
 @fixme Memory leak. See <fn:new_token>. */
static int notify_brief(const struct Scanner *const scan) {
	struct Token *tok;
	assert(scan);
	/* `brief` is just documentation; no code. */
	if(!(tok = new_token(&brief, scan))) fprintf(stderr,
		"%s: something went wrong with this operation.\n", oops(scan)), 0;
	return 1;
}

/* Output. */

/** Prints line info in a static buffer, (to be printed?) */
static const char *pos(const struct Token *const token) {
	static char p[128];
	if(!token) {
		sprintf(p, "Unknown position in report");
	} else {
		const int max_size = 16,
			tok_len = (token->length > max_size) ? max_size : token->length;
		sprintf(p, "%.32s:%lu, %s \"%.*s\"", token->label,
			(unsigned long)token->line, symbols[token->symbol], tok_len,
			token->from);
	}
	return p;
}

/** @return If the `code` is static. */
static int is_static(const struct TokenArray *const code) {
	const size_t code_size = TokenArraySize(code);
	const struct Token *const tokens = TokenArrayGet(code);
	assert(code);
	/* `main` is static, so hack it; we can't really do this because it's
	 general, but it works 99%. */
	return (code_size >= 1 && tokens[0].symbol == STATIC)
		|| (code_size >= 3
		&& tokens[0].symbol == ID && tokens[0].length == 3
		&& tokens[1].symbol == ID && tokens[1].length == 4
		&& tokens[2].symbol == LPAREN
		&& !strncmp(tokens[0].from, "int", 3)
		&& !strncmp(tokens[1].from, "main", 4));
}

/** @implements{Predicate<Segment>} */
static int keep_segment(const struct Segment *const s) {
	int keep = 0;
	assert(s);
	if(TokenArraySize(&s->doc) || AttributeArraySize(&s->attributes)
		|| s->division == DIV_FUNCTION) {
		struct Attribute *a = 0;
		/* `static` and containing `@allow`. */
		if(is_static(&s->code)) {
			while((a = AttributeArrayNext(&s->attributes, a))
				&& a->token.symbol != ATT_ALLOW);
			if(a) keep = 1;
		} else keep = 1;
	}
	/* But wait, everything except the preamble has to have a title! */
	if(s->division != DIV_PREAMBLE && !IndexArraySize(&s->code_params))
		keep = 0;
	if(!keep && CdocGetDebug() & DBG_ERASE) {
		char a[12];
		segment_to_string(s, &a);
		fprintf(stderr, "keep_segment: erasing %s.\n", a);
	}
	return keep;
}

/** Keeps only the stuff we care about; discards no docs except fn and `static`
 if not `@allow`. */
void ReportCull(void) {
	SegmentArrayKeepIf(&report, &keep_segment, &erase_segment);
}

#include "ReportOut.h"
#include "ReportWarning.h"
