# LexEngine guide

This guide explains how LexEngine turns text into tokens, how the different rules interact, and how to describe common kinds of languages. For exact signatures, see the [API reference](api.md).

- [The two pieces](#the-two-pieces)
- [Defining your token enum](#defining-your-token-enum)
- [How a token is chosen](#how-a-token-is-chosen)
- [Keywords](#keywords)
- [Symbols (operators and punctuation)](#symbols-operators-and-punctuation)
- [Identifiers](#identifiers)
- [Numbers](#numbers)
- [Strings](#strings)
- [Comments](#comments)
- [Newlines](#newlines)
- [Errors](#errors)
- [Positions](#positions)
- [Lifetimes](#lifetimes)
- [Recipes](#recipes)
- [Limitations](#limitations)

## The two pieces

LexEngine has two parts:

1. **`LangDef<TokenT>`** describes a language. You build it once, usually at startup, by chaining calls such as `.keyword(...)` and `.symbol(...)`. Mistakes in the definition, like a duplicate keyword, throw immediately.
2. **`Lexer<TokenT>`** reads one source string using a `LangDef` and hands out `Token<TokenT>` values one at a time.

One `LangDef` can be shared by any number of lexers, including on different threads, since lexing only reads it.

```cpp
#include "lexengine.hpp"

lexengine::LangDef<Tok> lang(Tok::Eof, Tok::Error);
// ... configure lang ...

lexengine::Lexer<Tok> lexer(lang, source);
while (!lexer.done()) {
    lexengine::Token<Tok> tok = lexer.next();
}

// Or get everything at once:
std::vector<lexengine::Token<Tok>> tokens = lexengine::tokenize(lang, source);
```

## Defining your token enum

Any enum works, scoped or not, with any underlying type. It needs one value for each kind of token you want to tell apart, plus two that LexEngine produces by itself:

- an **end-of-file** value, returned once the input is used up;
- an **error** value, returned for input that matches no rule.

Both are passed to the `LangDef` constructor, so they can't be forgotten:

```cpp
enum class Tok {
    If, Else, Return,             // keywords
    Plus, Minus, Assign, EqEq,    // symbols
    Ident, Int, Float, String,    // rule-based tokens
    Eof, Error,
};

lexengine::LangDef<Tok> lang(Tok::Eof, Tok::Error);
```

Several rules can share one enum value. For example, you could map both `"..."` and `'...'` strings to `Tok::String`.

## How a token is chosen

At each position, the lexer first skips whitespace (spaces, tabs, `\r`, `\v`, `\f`, and `\n` unless newlines are tokens). Then it tries the following in order and takes the **first** that applies:

| Step | Applies when | Produces |
| --- | --- | --- |
| 1. End of input | nothing is left | the EOF token |
| 2. Newline | newlines are tokens and the next character is `\n` or `\r\n` | the newline token |
| 3. Comment | a comment opener matches here | the comment token, or nothing (skipped) |
| 4. String | a string opener matches here | the string token |
| 5. Number | a number rule is set and the next character is a digit | an integer or float token |
| 6. Word | the next character can start an identifier | a keyword, word-shaped symbol, or identifier |
| 7. Symbol | a symbol matches here | the longest matching symbol |
| 8. Anything else | | an error token for one character |

The order explains several behaviours:

- **Comments come before symbols**, so with symbol `/` and comment `//`, the text `//` starts a comment instead of lexing as two slashes.
- **Strings come before words**, so a prefixed string opener such as `r"` works: `r"x"` is a string while a lone `r` is still an identifier.
- **Numbers come before words**, so digits never start an identifier, even with a custom identifier rule that allows them.

Within a step, the **longest** opener wins. If you define both `"` and `"""` as string openers, `"""` is chosen when it matches.

## Keywords

```cpp
lang.keyword("if", Tok::If)
    .keyword("else", Tok::Else);
```

A keyword only matches a **whole word**. The lexer reads a full identifier first and then checks whether it is a keyword, so `iffy` is an identifier, not `if` followed by `fy`. A word counts as a word according to your identifier rule (or the default rule if you didn't set one).

### Case-insensitive keywords

```cpp
lang.case_insensitive_keywords()   // must come before any keyword()
    .keyword("select", Tok::Select)
    .keyword("from", Tok::From);
```

`SELECT`, `Select` and `select` then all produce `Tok::Select`. The token's text is still exactly what appeared in the source. Calling `case_insensitive_keywords()` after a keyword has been added throws `std::logic_error`, because existing keywords would have been stored with the wrong case.

## Symbols (operators and punctuation)

```cpp
lang.symbol(">", Tok::Gt)
    .symbol(">>", Tok::Shr)
    .symbol(">>=", Tok::ShrAssign);
```

Symbols use **longest match**: `>>=` produces one `ShrAssign`, not `Shr` then `Assign`. You don't need to add them in any particular order.

Longest match is greedy. With the symbols above, `>>>` becomes `>>` then `>`. That is usually what you want, and it matches how C and most other languages behave.

### Word-shaped symbols

A symbol made of letters, such as `and` or `not`, works too. When the lexer reads a word, it checks keywords first, then whole-word symbols, then falls back to an identifier. So `and` produces your symbol, but `band` and `andy` are still identifiers.

## Identifiers

```cpp
lang.identifier(Tok::Ident);
```

By default an identifier starts with a letter or `_` and continues with letters, digits or `_`, as in C. Only ASCII letters count.

For other rules, pass an `IdentifierRule` with your own predicates:

```cpp
// Lisp-style: foo-bar, empty?, $var
lexengine::LangDef<Tok>::IdentifierRule rule{Tok::Ident};
rule.is_start = [](char c) { return std::isalpha((unsigned char)c) || c == '$'; };
rule.is_continue = [](char c) { return std::isalnum((unsigned char)c) || c == '-' || c == '?'; };
lang.identifier(rule);
```

`is_start` is checked once, for the first character; `is_continue` is checked for every character after it.

If you don't set an identifier rule, keywords and word-shaped symbols still work (using the default rule to find word boundaries), but any other word becomes a single error token.

## Numbers

```cpp
lang.numbers(Tok::Int, Tok::Float);
```

With the defaults, this accepts:

| Input | Token |
| --- | --- |
| `0`, `42`, `007` | integer |
| `3.14` | float |
| `1e10`, `1E+5`, `2.5e-3` | float |
| `0x1F`, `0XaB` | integer |

Turn features on or off with a `NumberRule`:

```cpp
lexengine::LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
rule.allow_binary = true;      // 0b1010 (off by default)
rule.allow_hex = false;        // no 0x...
rule.allow_float = false;      // integers only; also disables exponents
rule.allow_exponent = false;   // 2.5 is fine, 1e5 is not
rule.digit_separator = '\'';   // 1'000'000, like C++14 (or '_' for 1_000_000)
lang.numbers(rule);
```

Rules worth knowing:

- **A dot belongs to a number only if a digit follows it.** `1.5` is a float, but `1.` lexes as `1` then `.`. That keeps member access (`x.0`) and ranges (`1..2`) working. As a consequence, `.5` is not a float.
- **An exponent belongs to a number only if digits follow it.** `1e5` is a float; `1e` is an error.
- **A digit separator must sit between two digits.** `1_000` is fine; `1__0` and `1_` are errors.
- **A number glued to identifier characters is one error token.** `123abc`, `0xZZ`, `1.5f` and `0x` are each reported whole, instead of being split into confusing pieces.

## Strings

```cpp
lang.string_literal(Tok::String, "\"", "\"");
```

The token's text is the whole literal **including its delimiters and escape sequences exactly as written**. `"a\"b"` comes back as those six characters. Turning escape sequences into real characters is left to your parser, since every language defines them differently.

Customise a string kind with a `StringRule`:

```cpp
lexengine::LangDef<Tok>::StringRule raw{Tok::RawString, "r\"", "\""};
raw.escape = '\0';          // no escapes at all; '\\' by default
lang.string_literal(raw);

lexengine::LangDef<Tok>::StringRule block{Tok::String, "\"\"\"", "\"\"\""};
block.multiline = true;     // may contain newlines; false by default
lang.string_literal(block);
```

You can define as many string kinds as you like; the longest matching opener wins.

How a string body is read:

- The **escape character** skips the character after it, so `\"` doesn't close the string and `\\` is a literal backslash. An escape followed by a newline continues the string onto the next line, even in a single-line string.
- A **single-line** string that reaches a newline before its closing delimiter is an error. The error token stops before the newline, so lexing resumes on the next line.
- A string that reaches the end of input without closing is an error token covering the rest of the input.

## Comments

```cpp
lang.line_comment("//")
    .block_comment("/*", "*/");
```

Comments are skipped by default. A line comment runs up to, but not including, the end of the line, so if newlines are tokens you still get the newline token after it.

### Nested block comments

```cpp
lang.block_comment("/*", "*/", true);
```

With nesting, `/* a /* b */ c */` is a single comment. Without it (the default, as in C), the comment ends at the first `*/`. A nestable comment must have different open and close strings.

### Comments as tokens

If you need comments (for a formatter or documentation tool, for example), give the rule a token type:

```cpp
lexengine::LangDef<Tok>::CommentRule doc{"///", ""};   // empty close = line comment
doc.type = Tok::DocComment;
lang.comment(doc);
```

Because the longest opener wins, `///` and `//` can coexist: one is emitted and the other skipped.

An unterminated block comment is an error token covering the rest of the input.

## Newlines

For languages where line breaks matter, such as Python or shell, make newlines tokens:

```cpp
lang.newline(Tok::Newline);
```

Each `\n` or `\r\n` then produces one newline token. A lone `\r` is still treated as whitespace. Newlines inside block comments and multi-line strings are part of those tokens and don't produce newline tokens.

LexEngine doesn't produce indent and dedent tokens. A Python-style parser can compute them from the `column` of the first token after each newline.

## Errors

The lexer never throws. Input that doesn't fit the language becomes a token with your error type, and lexing carries on after it. This lets you report every problem in a file at once instead of stopping at the first one.

| Input | Error token text |
| --- | --- |
| A character no rule accepts | that one character (a whole UTF-8 character, not a single byte) |
| A malformed number such as `12ab` or `0x` | the whole malformed run |
| An unterminated string | from the opener to the end of the line (single-line) or input |
| An unterminated block comment | from the opener to the end of input |
| A word that isn't a keyword when no identifier rule is set | the whole word |

`LangDef` itself does throw, but only while you are building it, for mistakes in the definition. See the [API reference](api.md#errors-while-building) for the list.

## Positions

Every token has:

- **`line`**: 1-based.
- **`column`**: 1-based, counted in characters rather than bytes, so `é` (two bytes in UTF-8) counts as one column. A tab counts as one column.
- **`offset`**: the 0-based byte offset of the token's first byte in the source.

The EOF token is positioned just past the last character, which is useful for "unexpected end of file" messages.

## Lifetimes

Tokens don't copy text. `token.text` is a `std::string_view` pointing into your source, and the lexer keeps references to both the source and the `LangDef`. So:

- The **source** must stay alive as long as you use the lexer or any token from it.
- The **`LangDef`** must stay alive as long as you use the lexer.

The two most common mistakes are compile errors:

```cpp
Lexer<Tok> a(make_lang(), source);           // error: temporary LangDef
Lexer<Tok> b(lang, std::string("x + y"));    // error: temporary string
Lexer<Tok> c(lang, read_file(path));         // error if read_file returns std::string
```

Store the string in a variable first. Other ways to dangle, such as storing tokens and then destroying the source, can't be detected at compile time.

`Lexer::peek()` returns a reference that is only valid until the next call to `next()`.

## Recipes

### A C-like language

```cpp
lang.keyword("if", Tok::If).keyword("else", Tok::Else).keyword("while", Tok::While)
    .keyword("return", Tok::Return)
    .symbol("+", Tok::Plus).symbol("-", Tok::Minus).symbol("*", Tok::Star).symbol("/", Tok::Slash)
    .symbol("=", Tok::Assign).symbol("==", Tok::EqEq).symbol("!=", Tok::NotEq)
    .symbol("(", Tok::LParen).symbol(")", Tok::RParen)
    .symbol("{", Tok::LBrace).symbol("}", Tok::RBrace).symbol(";", Tok::Semi)
    .identifier(Tok::Ident)
    .numbers(Tok::Int, Tok::Float)
    .string_literal(Tok::String, "\"", "\"")
    .string_literal(Tok::Char, "'", "'")
    .line_comment("//")
    .block_comment("/*", "*/");
```

### A Python-like language

```cpp
lang.keyword("def", Tok::Def).keyword("if", Tok::If).keyword("return", Tok::Return)
    .symbol(":", Tok::Colon).symbol("(", Tok::LParen).symbol(")", Tok::RParen)
    .symbol("and", Tok::And).symbol("or", Tok::Or).symbol("not", Tok::Not)
    .identifier(Tok::Ident)
    .numbers(Tok::Int, Tok::Float)
    .string_literal(Tok::String, "'", "'")
    .string_literal(Tok::String, "\"", "\"")
    .line_comment("#")
    .newline(Tok::Newline);

lexengine::LangDef<Tok>::StringRule doc{Tok::String, "\"\"\"", "\"\"\""};
doc.multiline = true;
lang.string_literal(doc);
```

### A case-insensitive query language

```cpp
lang.case_insensitive_keywords()
    .keyword("select", Tok::Select).keyword("from", Tok::From).keyword("where", Tok::Where)
    .symbol(",", Tok::Comma).symbol("*", Tok::Star).symbol("=", Tok::Eq)
    .identifier(Tok::Ident)
    .numbers(Tok::Int, Tok::Float)
    .string_literal(Tok::String, "'", "'")
    .line_comment("--");
```

### Skipping error tokens versus stopping

```cpp
for (const auto& tok : lexengine::tokenize(lang, source)) {
    if (tok.type == Tok::Error)
        std::cerr << tok.line << ":" << tok.column << ": unexpected '" << tok.text << "'\n";
}
```

## Limitations

These are deliberately out of scope for now:

- **Escape processing.** String tokens contain the raw source text; decoding `\n`, `é` and so on is up to you.
- **Number suffixes** such as `10u` or `1.0f` are errors.
- **Leading-dot floats** such as `.5` lex as `.` then `5`.
- **Raw strings with custom delimiters,** like C++'s `R"x(...)x"`.
- **Indent and dedent tokens.** Derive them from token columns after newlines.
- **Non-ASCII identifiers.** The default identifier rule only accepts ASCII letters. A custom rule sees one byte at a time, so it can accept every byte of a UTF-8 character (`c & 0x80`) but can't check which character it is.
- **Context-sensitive lexing,** where the same text lexes differently depending on what came before (for example, regular-expression literals in JavaScript).
