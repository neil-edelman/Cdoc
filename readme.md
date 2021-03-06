# Cdoc\.c\.re #

 * [Description](#user-content-preamble)
 * [Function Summary](#user-content-summary)
 * [Function Definitions](#user-content-fn)
 * [License](#user-content-license)

## <a id = "user-content-preamble" name = "user-content-preamble">Description</a> ##

A context\-sensitive parser intended to process parts of a `C` compilation unit and extract documentation, as well as outputting that documentation into the format specified\. Designed to be very strict, warning one of documentation errors; and simple, made for self\-contained independent documenatation\. This does not do any compiling, just text\-parsing\. Thus, one can easily confuse by redefining symbols\. However, it assumes the macro `A_B_(Foo,Bar)` is transformed into `<A>Foo<B>Bar`\.

Documentation commands are `/` `**…` \(together\) and are ended with `*…/`, but not `/` `*…*` `/`, \(common code break\.\) Asterisks at the start of a line, like Kernel comments, or asterisks all over like some crazy ASCII art, are supported\. Documentation appearing at most two lines above `typedef`, `tag` \(`struct`, `enum`, `union`,\) data, and functions, is associated therewith; everything else is automatically inserted into the description\. Multiple documentation on the same command is appended\. Two hard returns is a paragraph\. Supports some `Markdown` commands included in the documentation,

 * `\` escapes `_~!@<>[]` and "\`"; "\`" can not be represented in math/code, \(multiple escapes aren't supported\); in paragraph mode, except in ambiguous cases, the only ones that are needed are \\\` and \\\_;
 * \_emphasised\_: _emphasised_;
 * \`code/math\`: `code/math`;
 * start lists with `\*` \(including spaces\) and end with a new paragraph; these are simple, can be anywhere and don't nest;
 * `\"` \(and optionally a space\) causes all the line after to be pre\-formatted;
 * Escapes included for convenience: `\,` "&#8239;" non\-breaking thin space, `\O` "&#927;" Bachmann–Landau notation, \(but really capital omicron because not many fonts have a shape for code\-point 120030,\) `\Theta` "&#920;", `\Omega` "&#937;", `\times` "&#215;", `\cdot` "&#183;"\.
 * `~` "&nbsp;" non\-breaking space;
 * `<url>`: relative URIs must have a slash or a dot to distinguish it from text;
 * `<Source, 1999, pp. 1-2>`: citation;
 * `<fn:<function>>`: function reference;
 * `<tag:<tag>>`: struct, union, or enum \(tag\) reference;
 * `<typedef:<typedef>>`: typedef reference;
 * `<data:<identifier>>`: data reference;
 * `[The link text](url)`: link, Markdown doesn't like `{}[]()` in text;
 * `![Caption text](url.image)`: image, same;
 * a local include directive has a documentation comment immediately after that reads only `\include`, it will also be included in the documentation\.

Each\-block\-tags separate the documentation until the next paragraph or until the next each\-block\-tag, and specify a specific documentation structure\. Each\-block\-tags that overlap are concatenated in the file order\. Not all of these are applicable for all segments of text\. These are:

 * `@subtitle`: only makes sense for preamble, \(it doesn't matter what case one writes it, but multiple are concatenated using semicolons\);
 * `@param[<param1>[, ...]]`: parameters, \(multiple are concatenated using spaces, so this really should be sentence case\);
 * `@author` \(commas\);
 * `@std`: standard, eg, `@std GNU-C99`, \(semicolons\);
 * `@depend`: dependancy, \(semicolons\);
 * `@fixme`: something doesn't work as expected, \(spaces\);
 * `@return`: normal function return, \(spaces\);
 * `@throws[<exception1>[, ...]]`: exceptional function return; `C` doesn't have native exceptions, so `@throws` means whatever one desires; perhaps a null pointer or false is returned and `errno` is set to `exception1`, \(spaces\);
 * `@implements`: `C` doesn't have the concept of implements, but we would say that a function having a prototype of `(int (*)(const void *, const void *))` implements `bsearch` and `qsort`, \(commas\);
 * `@order`: comments about the run\-time or space, \(spaces\);
 * and `@allow`, the latter being to allow `static` functions or data in the documentation, which are usually culled; one will be warned if this has any text\.

Perhaps the most striking difference from `Javadoc` and `Doxygen` is the `@param` has to be followed by a braced list, \(it's confusing to have the variable be indistinguishable from the text\.\)

If one sets `md` as output, it goes to `GitHub` Markdown that is specifically visible on the `GitHub` page, \(including working anchor links on browsers > 2000\.\) It bears little to Markdown supported in the documentation\.



 * Standard:  
   C89
 * Dependancies:  
   [re2c](http://re2c.org/)
 * Caveat:  
   Old\-style function support\. Trigraph support, \(haha\.\) Hide `const` on params when it can not affect function calls\. Documentation on functions should be added to documentation on prototypes with the same \(similar\) prototype\. Links to non\-documented code which sometimes doesn't show up, work without error, and create broken links\. 80\-characters _per_ line limit, [https://xxyxyz\.org/line\-breaking/](https://xxyxyz.org/line-breaking/), \(needs buffering\.\) Eg, fixme with no args disappears; we should NOT check if the string is empty for these values\. Better yet, have a flag\.


## <a id = "user-content-summary" name = "user-content-summary">Function Summary</a> ##

<table>

<tr><th>Modifiers</th><th>Function Name</th><th>Argument List</th></tr>

<tr><td align = right>enum Debug</td><td><a href = "#user-content-fn-155d6ff">CdocGetDebug</a></td><td></td></tr>

<tr><td align = right>enum Format</td><td><a href = "#user-content-fn-334aa1ab">CdocGetFormat</a></td><td></td></tr>

<tr><td align = right>const char *</td><td><a href = "#user-content-fn-7ee5d21c">CdocGetInput</a></td><td></td></tr>

<tr><td align = right>const char *</td><td><a href = "#user-content-fn-18fcd065">CdocGetOutput</a></td><td></td></tr>

</table>



## <a id = "user-content-fn" name = "user-content-fn">Function Definitions</a> ##

### <a id = "user-content-fn-155d6ff" name = "user-content-fn-155d6ff">CdocGetDebug</a> ###

<code>enum Debug <strong>CdocGetDebug</strong>(void)</code>

 * Return:  
   Whether the command\-line was set\.




### <a id = "user-content-fn-334aa1ab" name = "user-content-fn-334aa1ab">CdocGetFormat</a> ###

<code>enum Format <strong>CdocGetFormat</strong>(void)</code>

 * Return:  
   What format the output was specified to be in `enum Format`\. If there was no output format specified, guess\.




### <a id = "user-content-fn-7ee5d21c" name = "user-content-fn-7ee5d21c">CdocGetInput</a> ###

<code>const char *<strong>CdocGetInput</strong>(void)</code>

 * Return:  
   The input filename\.




### <a id = "user-content-fn-18fcd065" name = "user-content-fn-18fcd065">CdocGetOutput</a> ###

<code>const char *<strong>CdocGetOutput</strong>(void)</code>

 * Return:  
   The output filename\.






## <a id = "user-content-license" name = "user-content-license">License</a> ##

2019 Neil Edelman, distributed under the terms of the [MIT License](https://opensource.org/licenses/MIT)\.



