#include "harness.hpp"
#include "lexengine.hpp"

#include <cctype>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

std::size_t count_words_in_second_tu(std::string_view source);

namespace {

using lexengine::LangDef;
using lexengine::Lexer;

enum class Tok {
    If, Else, While, Return,
    Plus, Minus, Star, Slash, Assign, EqEq, Gt, Shr, ShrEq, Dot, LParen, RParen, LBrace, RBrace, Semi, And,
    Ident, Int, Float, Str, RawStr, Char, Comment, Newline, Eof, Error,
};

const char* tok_name(Tok t) {
    switch (t) {
        case Tok::If: return "If";
        case Tok::Else: return "Else";
        case Tok::While: return "While";
        case Tok::Return: return "Return";
        case Tok::Plus: return "Plus";
        case Tok::Minus: return "Minus";
        case Tok::Star: return "Star";
        case Tok::Slash: return "Slash";
        case Tok::Assign: return "Assign";
        case Tok::EqEq: return "EqEq";
        case Tok::Gt: return "Gt";
        case Tok::Shr: return "Shr";
        case Tok::ShrEq: return "ShrEq";
        case Tok::Dot: return "Dot";
        case Tok::LParen: return "LParen";
        case Tok::RParen: return "RParen";
        case Tok::LBrace: return "LBrace";
        case Tok::RBrace: return "RBrace";
        case Tok::Semi: return "Semi";
        case Tok::And: return "And";
        case Tok::Ident: return "Ident";
        case Tok::Int: return "Int";
        case Tok::Float: return "Float";
        case Tok::Str: return "Str";
        case Tok::RawStr: return "RawStr";
        case Tok::Char: return "Char";
        case Tok::Comment: return "Comment";
        case Tok::Newline: return "Newline";
        case Tok::Eof: return "Eof";
        case Tok::Error: return "Error";
    }
    return "?";
}

std::ostream& operator<<(std::ostream& os, Tok t) { return os << tok_name(t); }

// Type and text of one token, for compact expectations.
struct Tk {
    Tok type;
    std::string text;
    bool operator==(const Tk& o) const { return type == o.type && text == o.text; }
};

std::ostream& operator<<(std::ostream& os, const std::vector<Tk>& toks) {
    os << "[";
    for (std::size_t i = 0; i < toks.size(); ++i)
        os << (i ? ", " : "") << toks[i].type << " '" << toks[i].text << "'";
    return os << "]";
}

// Every token except the trailing EOF.
std::vector<Tk> lex(const LangDef<Tok>& lang, std::string_view source) {
    std::vector<Tk> out;
    for (const auto& t : lexengine::tokenize(lang, source)) {
        if (!t.text.empty())
            out.push_back({t.type, std::string(t.text)});
    }
    return out;
}

LangDef<Tok> c_like() {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.keyword("if", Tok::If)
        .keyword("else", Tok::Else)
        .keyword("while", Tok::While)
        .keyword("return", Tok::Return)
        .symbol("+", Tok::Plus)
        .symbol("-", Tok::Minus)
        .symbol("*", Tok::Star)
        .symbol("/", Tok::Slash)
        .symbol("=", Tok::Assign)
        .symbol("==", Tok::EqEq)
        .symbol(">", Tok::Gt)
        .symbol(">>", Tok::Shr)
        .symbol(">>=", Tok::ShrEq, "shift-assign")
        .symbol(".", Tok::Dot)
        .symbol("(", Tok::LParen)
        .symbol(")", Tok::RParen)
        .symbol("{", Tok::LBrace)
        .symbol("}", Tok::RBrace)
        .symbol(";", Tok::Semi)
        .identifier(Tok::Ident)
        .numbers(Tok::Int, Tok::Float)
        .string_literal(Tok::Str, "\"", "\"")
        .line_comment("//")
        .block_comment("/*", "*/");
    return lang;
}

// ---------------------------------------------------------------- LangDef

TEST(langdef_find_keyword) {
    auto lang = c_like();
    CHECK(lang.find_keyword("if") != nullptr);
    CHECK_EQ(lang.find_keyword("while")->type, Tok::While);
    CHECK(lang.find_keyword("If") == nullptr);
    CHECK(lang.find_keyword("iff") == nullptr);
    CHECK(lang.find_keyword("") == nullptr);
}

TEST(langdef_name_defaults_to_text) {
    auto lang = c_like();
    CHECK_EQ(lang.find_keyword("if")->name, std::string("if"));
    CHECK_EQ(lang.match_symbol(">>=")->name, std::string("shift-assign"));
    CHECK_EQ(lang.match_symbol("+")->name, std::string("+"));
}

TEST(langdef_match_symbol_is_longest_prefix) {
    auto lang = c_like();
    CHECK_EQ(lang.match_symbol(">>= 1")->type, Tok::ShrEq);
    CHECK_EQ(lang.match_symbol(">> 1")->type, Tok::Shr);
    CHECK_EQ(lang.match_symbol(">")->type, Tok::Gt);
    CHECK_EQ(lang.match_symbol("==")->type, Tok::EqEq);
    CHECK_EQ(lang.match_symbol("=>")->type, Tok::Assign);
    CHECK(lang.match_symbol("@") == nullptr);
    CHECK(lang.match_symbol("") == nullptr);
}

TEST(langdef_rejects_bad_keywords_and_symbols) {
    auto lang = c_like();
    CHECK_THROWS(lang.keyword("if", Tok::If), std::invalid_argument);
    CHECK_THROWS(lang.keyword("", Tok::If), std::invalid_argument);
    CHECK_THROWS(lang.symbol("+", Tok::Plus), std::invalid_argument);
    CHECK_THROWS(lang.symbol("", Tok::Plus), std::invalid_argument);
}

TEST(langdef_rejects_bad_rules) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    CHECK_THROWS(lang.string_literal(Tok::Str, "", "\""), std::invalid_argument);
    CHECK_THROWS(lang.string_literal(Tok::Str, "\"", ""), std::invalid_argument);
    CHECK_THROWS(lang.line_comment(""), std::invalid_argument);
    CHECK_THROWS(lang.block_comment("/*", ""), std::invalid_argument);
    CHECK_THROWS(lang.block_comment("", "*/"), std::invalid_argument);
    CHECK_THROWS(lang.block_comment("--", "--", true), std::invalid_argument);

    LangDef<Tok>::IdentifierRule rule{Tok::Ident};
    rule.is_start = nullptr;
    CHECK_THROWS(lang.identifier(rule), std::invalid_argument);
}

TEST(langdef_case_insensitive_must_come_first) {
    auto lang = c_like();
    CHECK_THROWS(lang.case_insensitive_keywords(), std::logic_error);

    LangDef<Tok> ci(Tok::Eof, Tok::Error);
    ci.case_insensitive_keywords().keyword("IF", Tok::If);
    CHECK(!ci.case_sensitive());
    CHECK(ci.find_keyword("if") != nullptr);
    CHECK(ci.find_keyword("iF") != nullptr);
    CHECK_EQ(ci.find_keyword("If")->text, std::string("if"));
    CHECK_THROWS(ci.keyword("if", Tok::If), std::invalid_argument);
}

TEST(langdef_match_string_and_comment_prefer_longest_opener) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.string_literal(Tok::Str, "\"", "\"")
        .string_literal(Tok::RawStr, "\"\"\"", "\"\"\"")
        .line_comment("-")
        .line_comment("--");
    CHECK_EQ(lang.match_string("\"\"\"abc")->type, Tok::RawStr);
    CHECK_EQ(lang.match_string("\"abc")->type, Tok::Str);
    CHECK(lang.match_string("abc") == nullptr);
    CHECK_EQ(lang.match_comment("-- x")->open, std::string("--"));
    CHECK_EQ(lang.match_comment("- x")->open, std::string("-"));
    CHECK(lang.match_comment("x") == nullptr);
}

TEST(langdef_accessors) {
    auto lang = c_like();
    CHECK_EQ(lang.eof_type(), Tok::Eof);
    CHECK_EQ(lang.error_type(), Tok::Error);
    CHECK(!lang.newline_type());
    CHECK_EQ(lang.identifier_rule()->type, Tok::Ident);
    CHECK_EQ(lang.number_rule()->float_type, Tok::Float);
    CHECK_EQ(lang.string_rules().size(), std::size_t(1));
    CHECK_EQ(lang.comment_rules().size(), std::size_t(2));
    lang.newline(Tok::Newline);
    CHECK_EQ(*lang.newline_type(), Tok::Newline);
}

TEST(langdef_accepts_enums_with_explicit_underlying_type) {
    enum class Small : std::uint8_t { Word, Eof, Error };
    LangDef<Small> lang(Small::Eof, Small::Error);
    lang.identifier(Small::Word);
    auto toks = lexengine::tokenize(lang, "a b");
    CHECK_EQ(toks.size(), std::size_t(3));
    CHECK(toks[0].type == Small::Word);
    CHECK(toks[2].type == Small::Eof);
}

// ---------------------------------------------------------------- Lexer basics

TEST(lexer_empty_source_is_just_eof) {
    auto lang = c_like();
    Lexer<Tok> lexer(lang, "");
    CHECK(!lexer.done());
    auto t = lexer.next();
    CHECK_EQ(t.type, Tok::Eof);
    CHECK(t.text.empty());
    CHECK_EQ(t.line, std::size_t(1));
    CHECK_EQ(t.column, std::size_t(1));
    CHECK_EQ(t.offset, std::size_t(0));
    CHECK(lexer.done());
}

TEST(lexer_keeps_returning_eof) {
    auto lang = c_like();
    Lexer<Tok> lexer(lang, "x");
    CHECK_EQ(lexer.next().type, Tok::Ident);
    CHECK_EQ(lexer.next().type, Tok::Eof);
    CHECK_EQ(lexer.next().type, Tok::Eof);
    CHECK(lexer.done());
}

TEST(lexer_whitespace_only) {
    auto lang = c_like();
    auto toks = lexengine::tokenize(lang, " \t\r\n\v\f ");
    CHECK_EQ(toks.size(), std::size_t(1));
    CHECK_EQ(toks[0].type, Tok::Eof);
    CHECK_EQ(toks[0].offset, std::size_t(7));
}

TEST(lexer_tokenize_ends_with_eof) {
    auto lang = c_like();
    auto toks = lexengine::tokenize(lang, "a + b");
    CHECK_EQ(toks.size(), std::size_t(4));
    CHECK_EQ(toks.back().type, Tok::Eof);
    CHECK_EQ(toks.back().offset, std::size_t(5));
}

TEST(lexer_peek_does_not_consume) {
    auto lang = c_like();
    Lexer<Tok> lexer(lang, "a b");
    CHECK_EQ(lexer.peek().text, std::string_view("a"));
    CHECK_EQ(lexer.peek().text, std::string_view("a"));
    CHECK_EQ(lexer.next().text, std::string_view("a"));
    CHECK_EQ(lexer.peek().text, std::string_view("b"));
    CHECK_EQ(lexer.next().text, std::string_view("b"));
    CHECK_EQ(lexer.peek().type, Tok::Eof);
    CHECK(!lexer.done());
    CHECK_EQ(lexer.next().type, Tok::Eof);
    CHECK(lexer.done());
}

TEST(lexer_tokenize_after_partial_consumption) {
    auto lang = c_like();
    Lexer<Tok> lexer(lang, "a b c");
    lexer.next();
    lexer.peek();
    auto rest = lexer.tokenize();
    CHECK_EQ(rest.size(), std::size_t(3));
    CHECK_EQ(rest[0].text, std::string_view("b"));
}

TEST(lexer_rejects_temporaries_at_compile_time) {
    using L = Lexer<Tok>;
    static_assert(std::is_constructible_v<L, const LangDef<Tok>&, std::string_view>);
    static_assert(std::is_constructible_v<L, const LangDef<Tok>&, const char*>);
    static_assert(std::is_constructible_v<L, const LangDef<Tok>&, std::string&>);
    static_assert(!std::is_constructible_v<L, LangDef<Tok>, std::string_view>);
    static_assert(!std::is_constructible_v<L, const LangDef<Tok>&, std::string>);
    CHECK(true);
}

// ---------------------------------------------------------------- Words

TEST(lexer_keywords_and_identifiers) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "if iffy _x x1 else While"),
             (std::vector<Tk>{{Tok::If, "if"}, {Tok::Ident, "iffy"}, {Tok::Ident, "_x"}, {Tok::Ident, "x1"},
                              {Tok::Else, "else"}, {Tok::Ident, "While"}}));
}

TEST(lexer_case_insensitive_keywords_keep_source_text) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.case_insensitive_keywords().keyword("if", Tok::If).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "IF If if iF ifx"),
             (std::vector<Tk>{{Tok::If, "IF"}, {Tok::If, "If"}, {Tok::If, "if"}, {Tok::If, "iF"}, {Tok::Ident, "ifx"}}));
}

TEST(lexer_word_shaped_symbols_beat_identifiers) {
    auto lang = c_like();
    lang.symbol("and", Tok::And);
    CHECK_EQ(lex(lang, "a and band andy"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::And, "and"}, {Tok::Ident, "band"}, {Tok::Ident, "andy"}}));
}

TEST(lexer_custom_identifier_rule) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::IdentifierRule rule{Tok::Ident};
    rule.is_start = [](char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '$'; };
    rule.is_continue = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '?'; };
    lang.identifier(rule).symbol("(", Tok::LParen).symbol(")", Tok::RParen);
    CHECK_EQ(lex(lang, "(empty? $list-a)"),
             (std::vector<Tk>{{Tok::LParen, "("}, {Tok::Ident, "empty?"}, {Tok::Ident, "$list-a"}, {Tok::RParen, ")"}}));
}

TEST(lexer_without_identifier_rule) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.keyword("if", Tok::If).symbol("+", Tok::Plus);
    CHECK_EQ(lex(lang, "if foo+if"),
             (std::vector<Tk>{{Tok::If, "if"}, {Tok::Error, "foo"}, {Tok::Plus, "+"}, {Tok::If, "if"}}));
}

// ---------------------------------------------------------------- Symbols

TEST(lexer_symbols_use_maximal_munch) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, ">>= >> > >>>= ==="),
             (std::vector<Tk>{{Tok::ShrEq, ">>="}, {Tok::Shr, ">>"}, {Tok::Gt, ">"}, {Tok::Shr, ">>"},
                              {Tok::Gt, ">"}, {Tok::Assign, "="}, {Tok::EqEq, "=="}, {Tok::Assign, "="}}));
}

TEST(lexer_symbols_without_spaces) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "f(a+b)*c;"),
             (std::vector<Tk>{{Tok::Ident, "f"}, {Tok::LParen, "("}, {Tok::Ident, "a"}, {Tok::Plus, "+"},
                              {Tok::Ident, "b"}, {Tok::RParen, ")"}, {Tok::Star, "*"}, {Tok::Ident, "c"},
                              {Tok::Semi, ";"}}));
}

TEST(lexer_unknown_characters_are_errors) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a @ b#"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Error, "@"}, {Tok::Ident, "b"}, {Tok::Error, "#"}}));
}

TEST(lexer_unknown_utf8_character_is_one_error) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a\xE2\x82\xAC" "b"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Error, "\xE2\x82\xAC"}, {Tok::Ident, "b"}}));
}

// ---------------------------------------------------------------- Numbers

TEST(lexer_integers_and_floats) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "0 42 007 3.14 1e10 1E+5 2.5e-3"),
             (std::vector<Tk>{{Tok::Int, "0"}, {Tok::Int, "42"}, {Tok::Int, "007"}, {Tok::Float, "3.14"},
                              {Tok::Float, "1e10"}, {Tok::Float, "1E+5"}, {Tok::Float, "2.5e-3"}}));
}

TEST(lexer_dot_needs_a_digit_after_it) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "1. 1..2 x.0"),
             (std::vector<Tk>{{Tok::Int, "1"}, {Tok::Dot, "."}, {Tok::Int, "1"}, {Tok::Dot, "."}, {Tok::Dot, "."},
                              {Tok::Int, "2"}, {Tok::Ident, "x"}, {Tok::Dot, "."}, {Tok::Int, "0"}}));
}

TEST(lexer_float_then_dot) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "1.5.3"), (std::vector<Tk>{{Tok::Float, "1.5"}, {Tok::Dot, "."}, {Tok::Int, "3"}}));
}

TEST(lexer_hex_numbers) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "0x1F 0XaB 0x 0xZ"),
             (std::vector<Tk>{{Tok::Int, "0x1F"}, {Tok::Int, "0XaB"}, {Tok::Error, "0x"}, {Tok::Error, "0xZ"}}));
}

TEST(lexer_binary_numbers_are_opt_in) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "0b101"), (std::vector<Tk>{{Tok::Error, "0b101"}}));

    LangDef<Tok> bin(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.allow_binary = true;
    bin.numbers(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(bin, "0b101 0B1 0b 0b12"),
             (std::vector<Tk>{{Tok::Int, "0b101"}, {Tok::Int, "0B1"}, {Tok::Error, "0b"}, {Tok::Error, "0b12"}}));
}

TEST(lexer_numbers_followed_by_letters_are_errors) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "123abc 1e 1.5x 9_"),
             (std::vector<Tk>{{Tok::Error, "123abc"}, {Tok::Error, "1e"}, {Tok::Error, "1.5x"}, {Tok::Error, "9_"}}));
}

TEST(lexer_exponent_sign_without_digits) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "1e+"), (std::vector<Tk>{{Tok::Error, "1e"}, {Tok::Plus, "+"}}));
}

TEST(lexer_digit_separators) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.digit_separator = '\'';
    lang.numbers(rule).identifier(Tok::Ident).symbol("'", Tok::Char);
    CHECK_EQ(lex(lang, "1'000'000 0xFF'FF 1'5.2'5 1'' 1'"),
             (std::vector<Tk>{{Tok::Int, "1'000'000"}, {Tok::Int, "0xFF'FF"}, {Tok::Float, "1'5.2'5"},
                              {Tok::Int, "1"}, {Tok::Char, "'"}, {Tok::Char, "'"}, {Tok::Int, "1"}, {Tok::Char, "'"}}));
}

TEST(lexer_underscore_separator_swallows_bad_runs) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.digit_separator = '_';
    lang.numbers(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "1_000 1__0 1_"),
             (std::vector<Tk>{{Tok::Int, "1_000"}, {Tok::Error, "1__0"}, {Tok::Error, "1_"}}));
}

TEST(lexer_floats_can_be_disabled) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.allow_float = false;
    lang.numbers(rule).symbol(".", Tok::Dot);
    CHECK_EQ(lex(lang, "1.5"), (std::vector<Tk>{{Tok::Int, "1"}, {Tok::Dot, "."}, {Tok::Int, "5"}}));
}

TEST(lexer_exponent_can_be_disabled) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.allow_exponent = false;
    lang.numbers(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "2.5 1e5"), (std::vector<Tk>{{Tok::Float, "2.5"}, {Tok::Error, "1e5"}}));
}

TEST(lexer_hex_can_be_disabled) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::NumberRule rule{Tok::Int, Tok::Float};
    rule.allow_hex = false;
    lang.numbers(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "0x10"), (std::vector<Tk>{{Tok::Error, "0x10"}}));
}

TEST(lexer_without_number_rule) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "a 12"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Error, "1"}, {Tok::Error, "2"}}));
}

// ---------------------------------------------------------------- Strings

TEST(lexer_strings) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, R"("" "abc" "a\"b" "a\\" x)"),
             (std::vector<Tk>{{Tok::Str, R"("")"}, {Tok::Str, R"("abc")"}, {Tok::Str, R"("a\"b")"},
                              {Tok::Str, R"("a\\")"}, {Tok::Ident, "x"}}));
}

TEST(lexer_string_contents_are_not_lexed) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, R"("if // /* 1.5")"), (std::vector<Tk>{{Tok::Str, R"("if // /* 1.5")"}}));
}

TEST(lexer_unterminated_string_stops_at_newline) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "\"abc\nx"), (std::vector<Tk>{{Tok::Error, "\"abc"}, {Tok::Ident, "x"}}));
}

TEST(lexer_unterminated_string_at_eof) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "x \"abc"), (std::vector<Tk>{{Tok::Ident, "x"}, {Tok::Error, "\"abc"}}));
    CHECK_EQ(lex(lang, "\"abc\\"), (std::vector<Tk>{{Tok::Error, "\"abc\\"}}));
}

TEST(lexer_escaped_newline_continues_string) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "\"a\\\nb\""), (std::vector<Tk>{{Tok::Str, "\"a\\\nb\""}}));
}

TEST(lexer_multiline_strings) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::StringRule rule{Tok::RawStr, "`", "`"};
    rule.multiline = true;
    lang.string_literal(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "`a\nb` c"), (std::vector<Tk>{{Tok::RawStr, "`a\nb`"}, {Tok::Ident, "c"}}));
}

TEST(lexer_strings_without_escapes) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::StringRule rule{Tok::Str, "'", "'"};
    rule.escape = '\0';
    lang.string_literal(rule).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, R"('a\' b)"), (std::vector<Tk>{{Tok::Str, R"('a\')"}, {Tok::Ident, "b"}}));
}

TEST(lexer_multiple_string_rules) {
    auto lang = c_like();
    lang.string_literal(Tok::Char, "'", "'");
    LangDef<Tok>::StringRule triple{Tok::RawStr, "\"\"\"", "\"\"\""};
    triple.multiline = true;
    lang.string_literal(triple);
    CHECK_EQ(lex(lang, "'c' \"s\" \"\"\"a\"b\nc\"\"\""),
             (std::vector<Tk>{{Tok::Char, "'c'"}, {Tok::Str, "\"s\""}, {Tok::RawStr, "\"\"\"a\"b\nc\"\"\""}}));
}

TEST(lexer_prefixed_string_opener_beats_identifier) {
    auto lang = c_like();
    lang.string_literal(Tok::RawStr, "r\"", "\"");
    CHECK_EQ(lex(lang, "r\"x\" r rx"),
             (std::vector<Tk>{{Tok::RawStr, "r\"x\""}, {Tok::Ident, "r"}, {Tok::Ident, "rx"}}));
}

// ---------------------------------------------------------------- Comments

TEST(lexer_line_comments_are_skipped) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a // b c\nd // at eof"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Ident, "d"}}));
}

TEST(lexer_comment_beats_symbol_prefix) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a / b //c\n/*d*/ /"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Slash, "/"}, {Tok::Ident, "b"}, {Tok::Slash, "/"}}));
}

TEST(lexer_block_comments_are_skipped) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a /* b\n c */ d/**/e"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Ident, "d"}, {Tok::Ident, "e"}}));
}

TEST(lexer_block_comments_do_not_nest_by_default) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "/* /* */ */"), (std::vector<Tk>{{Tok::Star, "*"}, {Tok::Slash, "/"}}));
}

TEST(lexer_nestable_block_comments) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.block_comment("/*", "*/", true).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "a /* x /* y */ z */ b"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Ident, "b"}}));
    CHECK_EQ(lex(lang, "/* /* */"), (std::vector<Tk>{{Tok::Error, "/* /* */"}}));
}

TEST(lexer_unterminated_block_comment_is_error) {
    auto lang = c_like();
    CHECK_EQ(lex(lang, "a /* b"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Error, "/* b"}}));
}

TEST(lexer_comments_as_tokens) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    LangDef<Tok>::CommentRule line{"#", ""};
    line.type = Tok::Comment;
    lang.comment(line).identifier(Tok::Ident);
    CHECK_EQ(lex(lang, "a # note\r\nb"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Comment, "# note"}, {Tok::Ident, "b"}}));
}

// ---------------------------------------------------------------- Newlines

TEST(lexer_newline_tokens) {
    auto lang = c_like();
    lang.newline(Tok::Newline);
    CHECK_EQ(lex(lang, "a\nb\r\n\nc"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Newline, "\n"}, {Tok::Ident, "b"}, {Tok::Newline, "\r\n"},
                              {Tok::Newline, "\n"}, {Tok::Ident, "c"}}));
}

TEST(lexer_lone_carriage_return_is_whitespace) {
    auto lang = c_like();
    lang.newline(Tok::Newline);
    CHECK_EQ(lex(lang, "a\rb"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Ident, "b"}}));
}

TEST(lexer_newline_after_line_comment_is_kept) {
    auto lang = c_like();
    lang.newline(Tok::Newline);
    CHECK_EQ(lex(lang, "a // x\r\nb"),
             (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Newline, "\r\n"}, {Tok::Ident, "b"}}));
}

TEST(lexer_newlines_inside_block_comments_and_strings_are_not_tokens) {
    auto lang = c_like();
    lang.newline(Tok::Newline);
    CHECK_EQ(lex(lang, "a /*\n*/ b"), (std::vector<Tk>{{Tok::Ident, "a"}, {Tok::Ident, "b"}}));
    CHECK_EQ(lex(lang, "\"a\\\nb\""), (std::vector<Tk>{{Tok::Str, "\"a\\\nb\""}}));
}

// ---------------------------------------------------------------- Positions

TEST(lexer_positions) {
    auto lang = c_like();
    auto toks = lexengine::tokenize(lang, "if x\n  y /* a\nb */ z\n");
    CHECK_EQ(toks.size(), std::size_t(5));
    CHECK_EQ(toks[0].line, std::size_t(1));
    CHECK_EQ(toks[0].column, std::size_t(1));
    CHECK_EQ(toks[1].column, std::size_t(4));
    CHECK_EQ(toks[1].offset, std::size_t(3));
    CHECK_EQ(toks[2].line, std::size_t(2));
    CHECK_EQ(toks[2].column, std::size_t(3));
    CHECK_EQ(toks[3].text, std::string_view("z"));
    CHECK_EQ(toks[3].line, std::size_t(3));
    CHECK_EQ(toks[3].column, std::size_t(6));
    CHECK_EQ(toks[4].type, Tok::Eof);
    CHECK_EQ(toks[4].line, std::size_t(4));
    CHECK_EQ(toks[4].column, std::size_t(1));
}

TEST(lexer_columns_count_code_points) {
    auto lang = c_like();
    // "é" is two bytes but one column.
    auto toks = lexengine::tokenize(lang, "\"\xC3\xA9\" x\tz");
    CHECK_EQ(toks[1].text, std::string_view("x"));
    CHECK_EQ(toks[1].column, std::size_t(5));
    CHECK_EQ(toks[1].offset, std::size_t(5));
    CHECK_EQ(toks[2].column, std::size_t(7));
}

TEST(lexer_token_text_points_into_source) {
    auto lang = c_like();
    std::string source = "abc + 12";
    auto toks = lexengine::tokenize(lang, source);
    for (const auto& t : toks) {
        CHECK(t.text.data() >= source.data());
        CHECK(t.text.data() + t.text.size() <= source.data() + source.size());
        CHECK_EQ(std::string_view(source).substr(t.offset, t.text.size()), t.text);
    }
}

// ---------------------------------------------------------------- Whole programs

TEST(lexer_small_program) {
    auto lang = c_like();
    const char* src =
        "// compute\n"
        "while (n >> 1) {\n"
        "    x = x * 2.5 + 0x10; /* step */\n"
        "    if (x == 3) return \"done\";\n"
        "}\n";
    CHECK_EQ(lex(lang, src),
             (std::vector<Tk>{{Tok::While, "while"}, {Tok::LParen, "("}, {Tok::Ident, "n"}, {Tok::Shr, ">>"},
                              {Tok::Int, "1"}, {Tok::RParen, ")"}, {Tok::LBrace, "{"},
                              {Tok::Ident, "x"}, {Tok::Assign, "="}, {Tok::Ident, "x"}, {Tok::Star, "*"},
                              {Tok::Float, "2.5"}, {Tok::Plus, "+"}, {Tok::Int, "0x10"}, {Tok::Semi, ";"},
                              {Tok::If, "if"}, {Tok::LParen, "("}, {Tok::Ident, "x"}, {Tok::EqEq, "=="},
                              {Tok::Int, "3"}, {Tok::RParen, ")"}, {Tok::Return, "return"}, {Tok::Str, "\"done\""},
                              {Tok::Semi, ";"}, {Tok::RBrace, "}"}}));
}

TEST(lexer_python_like_language) {
    LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.keyword("if", Tok::If)
        .keyword("return", Tok::Return)
        .symbol(":", Tok::Semi)
        .symbol("==", Tok::EqEq)
        .identifier(Tok::Ident)
        .numbers(Tok::Int, Tok::Float)
        .string_literal(Tok::Str, "'", "'")
        .line_comment("#")
        .newline(Tok::Newline);
    CHECK_EQ(lex(lang, "if x == 1:  # check\n    return 'y'\n"),
             (std::vector<Tk>{{Tok::If, "if"}, {Tok::Ident, "x"}, {Tok::EqEq, "=="}, {Tok::Int, "1"},
                              {Tok::Semi, ":"}, {Tok::Newline, "\n"}, {Tok::Return, "return"}, {Tok::Str, "'y'"},
                              {Tok::Newline, "\n"}}));
}

TEST(lexer_works_from_a_second_translation_unit) {
    CHECK_EQ(count_words_in_second_tu("one two three"), std::size_t(3));
}

}

int main(int argc, char** argv) {
    return harness::run_all(argc > 1 ? argv[1] : nullptr);
}
