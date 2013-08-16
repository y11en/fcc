#include "../inc/parser-helpers.h"

#include "../inc/debug.h"
#include "../inc/sym.h"
#include "../inc/ast.h"

#include "../inc/parser.h"

#include "stdlib.h"
#include "stdarg.h"
#include "string.h"

static char* tokenTagGetStr (tokenTag Token);

/*:::: SCOPE ::::*/

sym* scopeSet (parserCtx* ctx, sym* Scope) {
    sym* Old = ctx->scope;
    ctx->scope = Scope;
    return Old;
}

/*:::: ERROR MESSAGING ::::*/

static void error (parserCtx* ctx, const char* format, ...) {
    printf("error(%d:%d): ", ctx->location.line, ctx->location.lineChar);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    puts(".");

    ctx->errors++;
    debugWait();
}

void errorExpected (parserCtx* ctx, const char* Expected) {
    error(ctx, "expected %s, found '%s'", Expected, ctx->lexer->buffer);
}

void errorUndefSym (parserCtx* ctx) {
    error(ctx, "undefined symbol '%s'", ctx->lexer->buffer);
}

void errorUndefType (parserCtx* ctx) {
    error(ctx, "undefined symbol '%s', expected type", ctx->lexer->buffer);
}

void errorIllegalOutside (parserCtx* ctx, const char* what, const char* where) {
    error(ctx, "illegal %s outside of %s", what, where);
}

void errorRedeclaredSymAs (struct parserCtx* ctx, sym* Symbol, symTag tag) {
    const ast* first = (const ast*) vectorGet(&Symbol->decls, 0);

    error(ctx, "'%s' redeclared as %s.\n"
               "     (%d:%d): first declaration here as %s",
               Symbol->ident, symTagGetStr(tag),
               first->location.line, first->location.lineChar,
               symTagGetStr(Symbol->tag));
}

void errorReimplementedSym (struct parserCtx* ctx, sym* Symbol) {
    error(ctx, "%s '%s' reimplemented.\n"
               "     (%d:%d): first implementation here",
               Symbol->tag == symId ? "function" : symTagGetStr(Symbol->tag), Symbol->ident,
               Symbol->impl->location.line, Symbol->impl->location.lineChar);
}

/*:::: TOKEN HANDLING ::::*/

bool tokenIs (const parserCtx* ctx, const char* Match) {
    return !strcmp(ctx->lexer->buffer, Match);
}

bool tokenIsKeyword (const parserCtx* ctx, keywordTag keyword) {
    /*Keyword is keywordUndefined if not keyword token*/
    return ctx->lexer->keyword == keyword;
}

bool tokenIsIdent (const parserCtx* ctx) {
    return ctx->lexer->token == tokenIdent;
}

bool tokenIsInt (const parserCtx* ctx) {
    return ctx->lexer->token == tokenInt;
}

bool tokenIsString (const parserCtx* ctx) {
    return ctx->lexer->token == tokenStr;
}

bool tokenIsDecl (const parserCtx* ctx) {
    sym* Symbol = symFind(ctx->scope, ctx->lexer->buffer);

    return    (Symbol && Symbol->tag != symId
                      && Symbol->tag != symParam)
           || tokenIsKeyword(ctx, keywordConst)
           || tokenIsKeyword(ctx, keywordAuto) || tokenIsKeyword(ctx, keywordStatic)
           || tokenIsKeyword(ctx, keywordExtern) || tokenIsKeyword(ctx, keywordTypedef)
           || tokenIsKeyword(ctx, keywordStruct) || tokenIsKeyword(ctx, keywordUnion)
           || tokenIsKeyword(ctx, keywordEnum)
           || tokenIsKeyword(ctx, keywordVoid) || tokenIsKeyword(ctx, keywordBool)
           || tokenIsKeyword(ctx, keywordChar) || tokenIsKeyword(ctx, keywordInt);
}

void tokenNext (parserCtx* ctx) {
    ctx->location = lexerNext(ctx->lexer);
}

void tokenSkipMaybe (parserCtx* ctx) {
    if (!tokenIs(ctx, ")") && !tokenIs(ctx, "]") && !tokenIs(ctx, "}"))
        tokenNext(ctx);
}

void tokenMatch (parserCtx* ctx) {
    debugMsg("matched(%d:%d): '%s'",
             ctx->location.line,
             ctx->location.lineChar,
             ctx->lexer->buffer);
    tokenNext(ctx);
}

char* tokenDupMatch (parserCtx* ctx) {
    char* Old = strdup(ctx->lexer->buffer);

    tokenMatch(ctx);

    return Old;
}

/**
 * Return the string associated with a token tag
 */
static char* tokenTagGetStr (tokenTag tag) {
    if (tag == tokenUndefined) return "<undefined>";
    else if (tag == tokenOther) return "other";
    else if (tag == tokenEOF) return "end of file";
    else if (tag == tokenKeyword) return "keyword";
    else if (tag == tokenIdent) return "identifier";
    else if (tag == tokenInt) return "integer";
    else if (tag == tokenStr) return "string";
    else if (tag == tokenChar) return "character";
    else {
        char* str = malloc(logi(tag, 10)+2);
        sprintf(str, "%d", tag);
        debugErrorUnhandled("tokenTagGetStr", "token tag", str);
        free(str);
        return "<unhandled>";
    }
}

void tokenMatchKeyword (parserCtx* ctx, keywordTag keyword) {
    if (tokenIsKeyword(ctx, keyword))
        tokenMatch(ctx);

    else {
        const char* kw = keywordTagGetStr(keyword);
        char* kwInQuotes = malloc(strlen(kw)+3);
        sprintf(kwInQuotes, "'%s'", kw);
        errorExpected(ctx, kwInQuotes);
        free(kwInQuotes);

        tokenSkipMaybe(ctx);
    }
}

bool tokenTryMatchKeyword (struct parserCtx* ctx, keywordTag keyword) {
    if (tokenIsKeyword(ctx, keyword)) {
        tokenMatch(ctx);
        return true;

    } else
        return false;
}

void tokenMatchToken (parserCtx* ctx, tokenTag Match) {
    if (ctx->lexer->token == Match)
        tokenMatch(ctx);

    else {
        errorExpected(ctx, tokenTagGetStr(Match));
        tokenSkipMaybe(ctx);
    }
}

void tokenMatchStr (parserCtx* ctx, const char* Match) {
    if (tokenIs(ctx, Match))
        tokenMatch(ctx);

    else {
        char* expectedInQuotes = malloc(strlen(Match)+3);
        sprintf(expectedInQuotes, "'%s'", Match);
        errorExpected(ctx, expectedInQuotes);
        free(expectedInQuotes);

        tokenSkipMaybe(ctx);
    }
}

bool tokenTryMatchStr (parserCtx* ctx, const char* Match) {
    if (tokenIs(ctx, Match)) {
        tokenMatch(ctx);
        return true;

    } else
        return false;
}

int tokenMatchInt (parserCtx* ctx) {
    int ret = atoi(ctx->lexer->buffer);

    tokenMatchToken(ctx, tokenInt);

    return ret;
}

char* tokenMatchIdent (parserCtx* ctx) {
    char* Old = strdup(ctx->lexer->buffer);

    tokenMatchToken(ctx, tokenIdent);

    return Old;
}
