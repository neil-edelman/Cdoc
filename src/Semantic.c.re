/** 2019 Neil Edelman, distributed under the terms of the
 [MIT License](https://opensource.org/licenses/MIT).

 Guesses at decoding the meaning of a statement.

 @title Semantic.c.re
 @author Neil
 @version 2019-06
 @std C89
 @depend [re2c](http://re2c.org/)
 @fixme Old-style function support. */

#include <stdio.h>  /* printf */
/*#include <string.h>*/ /* memset */
/* #define NDEBUG */
/*#include <assert.h>*/ /* assert */
/*#include <limits.h>*/ /* INT_MAX */

#include "../src/XMacros.h" /* Needed for `#include "Namespace.h"`. */
#include "../src/Namespace.h"
#include "../src/Semantic.h" /* Semantic */


/** Not thread safe. */
struct Semantic {
	const char *buffer, *marker, *from, *cursor;
	/* fixme: The params; not used. */
	const char *s0, *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8;
} semantic;

static enum Namespace namespace(void);

/** @param[marks] A zero-terminated string composed of characters from
 `symbol_marks` in `Scanner.c.re_c` or null.
 @return An educated guess of the namespace for the first occurence. */
enum Namespace Semantic(const char *const marks) {
	enum Namespace name = NAME_PREAMBLE;
	if(!marks) return name;
	semantic.buffer = semantic.marker = semantic.from = semantic.cursor = marks;
	semantic.s0 = semantic.s1 = semantic.s2 = semantic.s3 = semantic.s4
		= semantic.s5 = semantic.s6 = semantic.s7 = semantic.s8 = 0;
	name = namespace();
	printf("--> Semantic: \"%s\" = %s.\n", marks, namespaces[name]);
	return name;
}

/*!re2c
re2c:yyfill:enable  = 0;
re2c:define:YYCTYPE  = char;
re2c:define:YYCURSOR = semantic.cursor;
re2c:define:YYMARKER = semantic.marker;

// Must match `SYMBOL` in `Scanner.c.re`.
// The symbols are "*=,;{}()[]#x123stzv.\x00" (as yet); the symbols that don't
// have a 1-to-1 correspendence are as follows:
operator = "*";
constant = "#";
word = "x";
tag = "s";
typedef = "t";
static = "z";
void = "v";
ellipses = ".";
end = "\x00";
name = word | typedef | static | void;
id = word
	| "1(" name ")"
	| "2(" name "," name ")"
	| "3(" name "," name "," name ")"; // Includes specific potential macros.

// These are derived.
star = [^\x00];
array = "[" (constant | word)? "]";
typename = word* (tag? id) (word* operator*)*;

definition = ("("|")"|operator|id|tag|void|ellipses|array)+;

left_things = "(" | "[" | "]" | operator | word; // const is an id
right_things = ")" | "[" | "]" | operator;

id_detail = ("("|"["|"]"operator);
param = word* typename array* id array*;
paramlist = ( param ("," param)* ("," ellipses)? ) | void;
fn = static? word* typename id "(" paramlist ")"; // fixme: or old-style

fn_ptr = (operator|id|tag|void)+ "(" operator (operator|word|tag)* id ")"
	"(" (operator|word|tag|void|"("|")"|array)* ")" array*;





storage_class_specifier = typedef | word | static;
type_qualifier = word;
pointer = ( operator type_qualifier* )+;

//
//declarator
//	: pointer direct_declarator
//	| direct_declarator
//	;
//
//direct_declarator = (id | "(" declarator ")")
//	( "[" constant_expression? "]"
//	| "(" (parameter_type_list | identifier_list)? ")")?
//
//struct_declarator
//	: declarator
//	| ':' constant_expression
//	| declarator ':' constant_expression
//	;

// struct_or_union_ or enum_
tag_specifier = tag id? ( "{"
	//(struct_declaration_list | enumerator_list)
	"}" )?;

// VOID | CHAR | SHORT | INT | LONG | FLOAT | DOUBLE | SIGNED | UNSIGNED
// | struct_or_union_specifier | enum_specifier | TYPE_NAME
type_specifier = word | id | tag_specifier;

//declaration_specifiers
//	: storage_class_specifier
//	| storage_class_specifier declaration_specifiers
//	| type_specifier
//	| type_specifier declaration_specifiers
//	| type_qualifier
//	| type_qualifier declaration_specifiers
//	;

//function_definition =
//	declaration_specifiers declarator declaration_list //compound_statement
//	| declaration_specifiers declarator //compound_statement
//	| declarator declaration_list //compound_statement
//	| declarator //compound_statement

*/

/** This really is very complcated and differs from one compiler and version to
 to next. We might !stags:re2c format = 'const char *@@;'; but that's really
 difficult. */
static enum Namespace namespace(void) {
	semantic.cursor = semantic.buffer;
	semantic.from = semantic.cursor;
/*!re2c
	"\x00" { return NAME_PREAMBLE; }
	typedef definition definition end { return NAME_TYPEDEF; }
	
	// These are tags, despite also being potentially data.
	static? word* tag id? end { return NAME_TAG; }

	// fixme: This does not take into account function pointers.

	static? (operator|id|tag|void)+ array* (operator *+)? end
		{ return NAME_GENERAL_DECLARATION; }
	")\x00" { return NAME_FUNCTION; }

	// fixme: Old-style function definitions.
	// fixme: int (*id)(void) would trivally break it.
	fn { return NAME_FUNCTION; }

	* { fprintf(stderr,
		"Semantic: wasn't able to determine the namespace; assuming data.\n");
		return NAME_GENERAL_DECLARATION; }
*/
}
