/* Warn if:
 -* Empty (or @allow full) attributes,
 -* Don't match the varibles.
 -* Haven't documented all the variables.
 */

static int attribute_use(const struct Attribute *const attribute,
	const int is_header, const int is_contents) {
	return is_header ^ !!TokenArraySize(&attribute->header)
		|| is_contents ^ !!TokenArraySize(&attribute->contents) ? 0 : 1;
}

static int attribute_okay(const struct Attribute *const attribute) {
	assert(attribute);
	switch(attribute->token.symbol) {
		case ATT_PARAM: /* `Scanner.c.re_c` has a state change. */
		case ATT_THROWS: return attribute_use(attribute, 1, 1);
		case ATT_TITLE: /* Otherwise, warn if empty. */
		case ATT_AUTHOR:
		case ATT_STD:
		case ATT_DEPEND:
		case ATT_VERSION:
		case ATT_FIXME:
		case ATT_RETURN:
		case ATT_IMPLEMENTS:
		case ATT_ORDER: return attribute_use(attribute, 0, 1);
		case ATT_ALLOW: return attribute_use(attribute, 0, 0); /* Or full. */
		default: return 0;
	}
}

/** Seaches for `match` in the `params` supplied by the parser. */
static int match_function_params(const struct Token *const match,
	const struct TokenRefArray *const params) {
	struct Token **param = TokenRefArrayNext(params, 0); /* The name. */
	char a[12];
	assert(match && params);
	token_to_string(match, &a);
	while((param = TokenRefArrayNext(params, param)))
		if(!token_compare(match, *param)) return 1;
	return 0;
}

/** Seaches for `match` in the param header of `attributes`
 (eg, `@param[here, var]`) of the documentation. */
static int match_param_attributes(const struct Token *const match,
	const struct AttributeArray *const attributes) {
	struct Attribute *attribute = 0;
	assert(match && attributes);
	while((attribute = AttributeArrayNext(attributes, attribute))) {
		struct Token *param = 0;
		if(attribute->token.symbol != ATT_PARAM) continue;
		while((param = TokenArrayNext(&attribute->header, param)))
			if(!token_compare(match, param)) return 1;
	}
	return 0;
}

static void warn_segment(const struct Segment *const segment) {
	/*const size_t doc_size = TokenArraySize(&segment->doc),
		code_size = TokenArraySize(&segment->code);*/
	struct Attribute *attribute = 0;
	struct Token **param;
	/* Check for empty (or full, as the case may be) attributes. */
	while((attribute = AttributeArrayNext(&segment->attributes, attribute)))
		if(!attribute_okay(attribute))
		fprintf(stderr, "%s: attribute not okay.\n", pos(&attribute->token));
	switch(segment->division) {
	case DIV_FUNCTION:
		/* Check for extraneous params. */
		attribute = 0;
		while((attribute = AttributeArrayNext(&segment->attributes, attribute)))
		{
			struct Token *match = 0;
			if(attribute->token.symbol != ATT_PARAM) continue;
			while((match = TokenArrayNext(&attribute->header, match)))
				if(!match_function_params(match, &segment->params))
				fprintf(stderr, "%s: extraneous variable.\n", pos(match));
		}
		/* Check for params that are undocumented. */
		param = TokenRefArrayNext(&segment->params, 0);
		while((param = TokenRefArrayNext(&segment->params, param))) {
			/* fixme: also check the doc and return. */
			if(!match_param_attributes(*param, &segment->attributes))
				fprintf(stderr, "%s: variable may be undocumented.\n",
				pos(*param));
		}
	case DIV_PREAMBLE:
	case DIV_TAG:
	case DIV_TYPEDEF:
	case DIV_DATA:
		/* It's nothing. */
		if(!TokenArraySize(&segment->doc)
			&& !AttributeArraySize(&segment->attributes)) return;
	}
}

void ReportWarn(void) {
	struct Segment *segment = 0;
	while((segment = SegmentArrayNext(&report, segment)))
		warn_segment(segment);
}