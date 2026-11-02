# LexEngine

A header-only C++17 library for building a lexer for your own language without writing one by hand.

You describe the language — its keywords, operators, identifiers, numbers, strings and comments — and LexEngine turns source text into a stream of tokens using **your own token enum**.

```cpp
#include "lexengine.hpp"

enum class Tok { Let, Ident, Number, Float, Plus, Star, Assign, LParen, RParen, Eof, Error };

lexengine::LangDef<Tok> lang(Tok::Eof, Tok::Error);
lang.keyword("let", Tok::Let)
    .symbol("+", Tok::Plus)
    .symbol("*", Tok::Star)
    .symbol("=", Tok::Assign)
    .symbol("(", Tok::LParen)
    .symbol(")", Tok::RParen)
    .identifier(Tok::Ident)
    .numbers(Tok::Number, Tok::Float)
    .line_comment("#");

std::string source = "let x = 2 * (y + 1.5) # comment";
lexengine::Lexer<Tok> lexer(lang, source);
while (!lexer.done()) {
    auto tok = lexer.next();
    // tok.type, tok.text, tok.line, tok.column, tok.offset
}
```

The full version is in [`examples/calc.cpp`](examples/calc.cpp). Running it (`make example && ./build/calc`) prints each token as `line:column  type  'text'`:

```
1:1  Let  'let'
1:5  Ident  'x'
1:7  Assign  '='
1:9  Number  '2'
1:11  Star  '*'
1:13  LParen  '('
1:14  Ident  'y'
1:16  Plus  '+'
1:18  Float  '1.5'
1:21  RParen  ')'
1:32  Eof  ''
```

The comment is skipped, and the stream always ends with your end-of-file token.

## Features

- **Your token type.** Pass any enum; every token comes back typed with it.
- **Keywords**, optionally case-insensitive.
- **Operators and punctuation** with longest-match ("maximal munch"): `>>=` beats `>>` beats `>`.
- **Identifiers** with default C-style rules or your own start/continue predicates.
- **Numbers**: integers, floats, exponents, hex, binary, and digit separators such as `1'000` or `1_000`.
- **Strings**: any number of string kinds with custom delimiters, escape characters, and single- or multi-line bodies.
- **Comments**: line and block comments, optionally nestable, either skipped or emitted as tokens.
- **Significant newlines** for indentation- or line-based languages.
- **Positions**: every token carries line, column (in UTF-8 characters) and byte offset.
- **No exceptions while lexing.** Bad input becomes an error token and lexing continues, so you can report several problems at once.
- **Zero-copy.** Token text is a `std::string_view` into your source.

## Installation

LexEngine is header-only with no dependencies beyond the standard library. Copy `langdef.hpp`, `lexer.hpp` and `lexengine.hpp` into your project and include `lexengine.hpp`.

Requires C++17 or later.

## Documentation

- [Guide](docs/guide.md) — how lexing works, how rules interact, and recipes for common languages.
- [API reference](docs/api.md) — every type and function.

## Building the tests and example

```sh
make test        # run the test suite
make sanitize    # run it under AddressSanitizer and UBSan
make example     # build examples/calc.cpp into build/calc
make test FILTER=string    # run only tests whose names contain "string"
```

The test suite has no dependencies; it uses a small harness in `tests/harness.hpp`.

## Project layout

| Path | Contents |
| --- | --- |
| `lexengine.hpp` | Include this. Pulls in the two headers below. |
| `langdef.hpp` | `LangDef<TokenT>`: the language definition. |
| `lexer.hpp` | `Lexer<TokenT>`, `Token<TokenT>` and `tokenize()`. |
| `examples/` | A runnable example. |
| `tests/` | The test suite. |
| `docs/` | Guide and API reference. |
