# LexEngine API reference

Everything lives in namespace `lexengine`. Include `lexengine.hpp` to get the whole library. For explanations and examples, see the [guide](guide.md).

- [`LangDef<TokenT>`](#langdeftokent)
  - [Constructor](#constructor)
  - [Building methods](#building-methods)
  - [Rule structs](#rule-structs)
  - [Query methods](#query-methods)
  - [Errors while building](#errors-while-building)
- [`Token<TokenT>`](#tokentokent)
- [`Lexer<TokenT>`](#lexertokent)
- [`tokenize()`](#tokenize)

---

## `LangDef<TokenT>`

```cpp
template <typename TokenT>
class LangDef;
```

Describes a language. `TokenT` must be an enum type; anything else fails a `static_assert`.

A `LangDef` is copyable and movable. Lexing only reads it, so one instance can be used by many lexers at once, including on different threads, as long as nobody modifies it at the same time.

### Constructor

```cpp
LangDef(TokenT eof, TokenT error);
```

| Parameter | Meaning |
| --- | --- |
| `eof` | Type of the token returned at the end of input. |
| `error` | Type of the token returned for input no rule accepts. |

### Building methods

Every building method returns `LangDef&`, so calls can be chained.

#### `keyword`

```cpp
LangDef& keyword(std::string text, TokenT type, std::string name = {});
```

Adds a keyword. It matches only a whole word, as determined by the identifier rule. `name` is an optional label for diagnostics; it defaults to `text`.

Throws `std::invalid_argument` if `text` is empty or the keyword already exists (compared case-insensitively if `case_insensitive_keywords()` was called).

#### `symbol`

```cpp
LangDef& symbol(std::string text, TokenT type, std::string name = {});
```

Adds an operator or punctuation symbol. The longest symbol that matches at the current position wins. A symbol made of identifier characters (such as `and`) matches only as a whole word, and takes priority over identifiers. `name` defaults to `text`.

Throws `std::invalid_argument` if `text` is empty or the symbol already exists.

#### `identifier`

```cpp
LangDef& identifier(TokenT type);
LangDef& identifier(IdentifierRule rule);
```

Enables identifiers. The first form uses the default C-style rule. Calling it again replaces the previous rule.

Throws `std::invalid_argument` if either predicate in `rule` is empty.

#### `numbers`

```cpp
LangDef& numbers(TokenT int_type, TokenT float_type);
LangDef& numbers(NumberRule rule);
```

Enables number literals. The first form uses the default `NumberRule` settings. Calling it again replaces the previous rule.

#### `string_literal`

```cpp
LangDef& string_literal(TokenT type, std::string open, std::string close);
LangDef& string_literal(StringRule rule);
```

Adds a kind of string literal. Can be called several times; the longest matching opener wins. The first form uses `\` as the escape character and disallows newlines.

Throws `std::invalid_argument` if `open` or `close` is empty.

#### `line_comment`

```cpp
LangDef& line_comment(std::string open);
```

Adds a comment that runs from `open` to the end of the line. It is skipped.

Throws `std::invalid_argument` if `open` is empty.

#### `block_comment`

```cpp
LangDef& block_comment(std::string open, std::string close, bool nestable = false);
```

Adds a comment that runs from `open` to `close`. It is skipped. If `nestable`, each inner `open` needs its own `close`.

Throws `std::invalid_argument` if `open` or `close` is empty, or if `nestable` is true and `open == close`.

#### `comment`

```cpp
LangDef& comment(CommentRule rule);
```

Adds a comment from a full rule. Use this to emit comments as tokens. Can be called several times; the longest matching opener wins.

Throws `std::invalid_argument` if `rule.open` is empty, or if `rule.nestable` is true and `rule.open == rule.close`.

#### `newline`

```cpp
LangDef& newline(TokenT type);
```

Makes each `\n` or `\r\n` outside a comment or string produce a token of `type`. Without this, newlines are whitespace.

#### `case_insensitive_keywords`

```cpp
LangDef& case_insensitive_keywords();
```

Makes keyword matching ignore ASCII case. Token text still shows the original spelling.

Throws `std::logic_error` if any keyword has already been added.

### Rule structs

These are nested in `LangDef<TokenT>`, for example `lexengine::LangDef<Tok>::NumberRule`. All are aggregates: set the leading `TokenT` fields with brace initialisation, then change the remaining fields as needed.

#### `SymbolInfo`

Describes a keyword or symbol. Returned by `find_keyword()` and `match_symbol()`.

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | `TokenT` | Token type produced. |
| `text` | `std::string` | The text as stored (lowercase for case-insensitive keywords). |
| `name` | `std::string` | Diagnostic label; `text` unless one was given. |

#### `IdentifierRule`

```cpp
LangDef<Tok>::IdentifierRule rule{Tok::Ident};
```

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | `TokenT` | — | Token type produced. |
| `is_start` | `std::function<bool(char)>` | ASCII letter or `_` | May this character begin an identifier? |
| `is_continue` | `std::function<bool(char)>` | ASCII letter, digit or `_` | May this character follow the first? |

The predicates receive one byte at a time.

#### `NumberRule`

```cpp
LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
```

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `int_type` | `TokenT` | — | Token type for integers. |
| `float_type` | `TokenT` | — | Token type for numbers with a fraction or exponent. |
| `allow_float` | `bool` | `true` | Accept a fraction (`1.5`). `false` also disables exponents. |
| `allow_exponent` | `bool` | `true` | Accept an exponent (`1e5`, `2.5E-3`). |
| `allow_hex` | `bool` | `true` | Accept `0x`/`0X` integers. |
| `allow_binary` | `bool` | `false` | Accept `0b`/`0B` integers. |
| `digit_separator` | `char` | `'\0'` (none) | Character allowed between two digits, such as `'\''` or `'_'`. |

#### `StringRule`

```cpp
LangDef<Tok>::StringRule rule{Tok::String, "\"", "\""};
```

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | `TokenT` | — | Token type produced. |
| `open` | `std::string` | — | Opening delimiter. |
| `close` | `std::string` | — | Closing delimiter. |
| `escape` | `char` | `'\\'` | Character that makes the next character literal. `'\0'` disables escapes. |
| `multiline` | `bool` | `false` | Whether the body may contain unescaped newlines. |

#### `CommentRule`

```cpp
LangDef<Tok>::CommentRule rule{"/*", "*/"};
```

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `open` | `std::string` | — | Opening delimiter. |
| `close` | `std::string` | — | Closing delimiter. Empty means the comment runs to the end of the line. |
| `nestable` | `bool` | `false` | Whether inner `open`s must each be closed. |
| `type` | `std::optional<TokenT>` | empty | Token type to emit. Empty means the comment is skipped. |

### Query methods

These are what `Lexer` uses. They're public so you can build your own lexer or tooling on top of a `LangDef`.

| Method | Returns |
| --- | --- |
| `const SymbolInfo* find_keyword(std::string_view word) const` | The keyword exactly equal to `word` (respecting case sensitivity), or `nullptr`. |
| `const SymbolInfo* match_symbol(std::string_view rest) const` | The longest symbol that is a prefix of `rest`, or `nullptr`. |
| `const CommentRule* match_comment(std::string_view rest) const` | The comment rule with the longest opener that is a prefix of `rest`, or `nullptr`. |
| `const StringRule* match_string(std::string_view rest) const` | The string rule with the longest opener that is a prefix of `rest`, or `nullptr`. |
| `const IdentifierRule* identifier_rule() const` | The identifier rule, or `nullptr` if none is set. |
| `const NumberRule* number_rule() const` | The number rule, or `nullptr` if none is set. |
| `const std::vector<StringRule>& string_rules() const` | All string rules, in the order added. |
| `const std::vector<CommentRule>& comment_rules() const` | All comment rules, in the order added. |
| `const std::optional<TokenT>& newline_type() const` | The newline token type, or empty if newlines are whitespace. |
| `TokenT eof_type() const` | The end-of-file token type. |
| `TokenT error_type() const` | The error token type. |
| `bool case_sensitive() const` | `false` after `case_insensitive_keywords()`. |

Returned pointers and references stay valid until the `LangDef` is modified or destroyed.

### Errors while building

| Exception | Thrown by | When |
| --- | --- | --- |
| `std::invalid_argument` | `keyword`, `symbol` | Empty text, or a duplicate. |
| `std::invalid_argument` | `identifier` | An `IdentifierRule` with an empty predicate. |
| `std::invalid_argument` | `string_literal` | Empty `open` or `close`. |
| `std::invalid_argument` | `line_comment`, `block_comment`, `comment` | Empty `open`; empty `close` for `block_comment`; nestable with `open == close`. |
| `std::logic_error` | `case_insensitive_keywords` | Called after a keyword was added. |

---

## `Token<TokenT>`

```cpp
template <typename TokenT>
struct Token {
    TokenT type;
    std::string_view text;
    std::size_t line;
    std::size_t column;
    std::size_t offset;
};
```

| Field | Meaning |
| --- | --- |
| `type` | The token's type, from your enum. |
| `text` | The exact source text of the token, pointing into the source. String tokens include their delimiters. Empty only for the EOF token. |
| `line` | 1-based line of the first character. |
| `column` | 1-based column of the first character, counted in UTF-8 characters. |
| `offset` | 0-based byte offset of the first character. |

---

## `Lexer<TokenT>`

```cpp
template <typename TokenT>
class Lexer;
```

Produces tokens from one source string.

### Constructor

```cpp
Lexer(const LangDef<TokenT>& lang, std::string_view source);
```

Both `lang` and `source` are referenced, not copied, and must outlive the lexer and every token it returns. Passing a temporary `LangDef` or a temporary `std::string` is a compile error.

### Methods

| Method | Description |
| --- | --- |
| `Token<TokenT> next()` | Returns the next token. After the end of input it returns the EOF token, and keeps returning it on later calls. |
| `const Token<TokenT>& peek()` | Returns the next token without consuming it. The reference is valid until the next call to `next()`. |
| `bool done() const` | `true` once `next()` has returned the EOF token. Peeking at EOF does not set it. |
| `std::vector<Token<TokenT>> tokenize()` | Returns all remaining tokens, including a peeked one, ending with the EOF token. |

### Typical loop

```cpp
lexengine::Lexer<Tok> lexer(lang, source);
while (!lexer.done()) {
    auto tok = lexer.next();
    // ...
}
```

This loop also handles the EOF token, which is returned on the final iteration.

---

## `tokenize()`

```cpp
template <typename TokenT>
std::vector<Token<TokenT>> tokenize(const LangDef<TokenT>& lang, std::string_view source);
```

Lexes all of `source` and returns every token, ending with the EOF token. Equivalent to `Lexer<TokenT>(lang, source).tokenize()`. The same lifetime rules apply: `source` must outlive the returned tokens, and temporaries are rejected at compile time.
