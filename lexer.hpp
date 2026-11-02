#pragma once

#include "langdef.hpp"

#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace lexengine {

template <typename TokenT>
struct Token {
    TokenT type;
    std::string_view text;  // slice of the source; string tokens keep their delimiters
    std::size_t line;       // 1-based
    std::size_t column;     // 1-based, counted in UTF-8 code points
    std::size_t offset;     // byte offset into the source
};

// Lexes one source string. The LangDef and the source must outlive the lexer
// and every token it returns, because tokens point into the source.
template <typename TokenT>
class Lexer {
    public:
        using LangDefT = LangDef<TokenT>;
        using TokenType = Token<TokenT>;

        Lexer(const LangDefT& lang, std::string_view source) : lang_(lang), src_(source) {}

        // Reject temporaries that would leave the lexer dangling.
        Lexer(const LangDefT&&, std::string_view) = delete;
        template <typename S, typename = std::enable_if_t<std::is_same_v<S, std::string>>>
        Lexer(const LangDefT&, S&&) = delete;

        // Returns the next token. After the end of input, keeps returning the EOF token.
        TokenType next() {
            TokenType tok = peeked_ ? *peeked_ : lex();
            peeked_.reset();
            // EOF is the only token with empty text.
            if (tok.text.empty())
                finished_ = true;
            return tok;
        }

        const TokenType& peek() {
            if (!peeked_)
                peeked_ = lex();
            return *peeked_;
        }

        // True once next() has returned the EOF token.
        bool done() const { return finished_; }

        // All remaining tokens, ending with the EOF token.
        std::vector<TokenType> tokenize() {
            std::vector<TokenType> out;
            do {
                out.push_back(next());
            } while (!finished_);
            return out;
        }

    private:
        using NumberRule = typename LangDefT::NumberRule;
        using StringRule = typename LangDefT::StringRule;
        using CommentRule = typename LangDefT::CommentRule;

        TokenType lex() {
            for (;;) {
                skip_whitespace();
                start_ = pos_;
                start_line_ = line_;
                start_column_ = column_;

                if (at_end())
                    return emit(lang_.eof_type());

                char c = src_[pos_];

                // skip_whitespace() only stops on a newline when newlines are tokens.
                if (c == '\n' || c == '\r') {
                    advance(c == '\r' ? 2 : 1);
                    return emit(*lang_.newline_type());
                }

                if (const CommentRule* rule = lang_.match_comment(rest())) {
                    if (!lex_comment(*rule))
                        return emit(lang_.error_type());
                    if (rule->type)
                        return emit(*rule->type);
                    continue;
                }

                if (const StringRule* rule = lang_.match_string(rest()))
                    return emit(lex_string(*rule) ? rule->type : lang_.error_type());

                if (lang_.number_rule() && is_digit(c))
                    return lex_number(*lang_.number_rule());

                if (ident_start(c))
                    return lex_word();

                if (const auto* sym = lang_.match_symbol(rest())) {
                    advance(sym->text.size());
                    return emit(sym->type);
                }

                advance_code_point();
                return emit(lang_.error_type());
            }
        }

        void skip_whitespace() {
            bool newline_tokens = lang_.newline_type().has_value();
            while (!at_end()) {
                char c = src_[pos_];
                if (c == ' ' || c == '\t' || c == '\v' || c == '\f') {
                    advance(1);
                } else if (c == '\n') {
                    if (newline_tokens)
                        return;
                    advance(1);
                } else if (c == '\r') {
                    if (newline_tokens && peek_char(1) == '\n')
                        return;
                    advance(1);
                } else {
                    return;
                }
            }
        }

        bool lex_comment(const CommentRule& rule) {
            advance(rule.open.size());

            // Line comments stop before the line break so it can still become a newline token.
            if (rule.close.empty()) {
                while (!at_end() && src_[pos_] != '\n' && !(src_[pos_] == '\r' && peek_char(1) == '\n'))
                    advance(1);
                return true;
            }

            std::size_t depth = 1;
            while (!at_end()) {
                if (starts_with(rule.close)) {
                    advance(rule.close.size());
                    if (--depth == 0)
                        return true;
                } else if (rule.nestable && starts_with(rule.open)) {
                    advance(rule.open.size());
                    ++depth;
                } else {
                    advance(1);
                }
            }
            return false;
        }

        bool lex_string(const StringRule& rule) {
            advance(rule.open.size());
            while (!at_end()) {
                char c = src_[pos_];
                if (rule.escape != '\0' && c == rule.escape && pos_ + 1 < src_.size()) {
                    advance(2);
                } else if (starts_with(rule.close)) {
                    advance(rule.close.size());
                    return true;
                } else if (c == '\n' && !rule.multiline) {
                    return false;
                } else {
                    advance(1);
                }
            }
            return false;
        }

        TokenType lex_number(const NumberRule& rule) {
            bool ok = true;
            bool is_float = false;
            char prefix = static_cast<char>(std::tolower(static_cast<unsigned char>(peek_char(1))));

            if (src_[pos_] == '0' && rule.allow_hex && prefix == 'x') {
                advance(2);
                ok = scan_digits(rule, is_hex_digit);
            } else if (src_[pos_] == '0' && rule.allow_binary && prefix == 'b') {
                advance(2);
                ok = scan_digits(rule, is_binary_digit);
            } else {
                scan_digits(rule, is_digit);
                // A dot only belongs to the number if a digit follows, so `1.foo` and `1..2` still work.
                if (rule.allow_float && peek_char(0) == '.' && is_digit(peek_char(1))) {
                    advance(1);
                    scan_digits(rule, is_digit);
                    is_float = true;
                }
                if (rule.allow_float && rule.allow_exponent && (peek_char(0) == 'e' || peek_char(0) == 'E')) {
                    std::size_t sign = (peek_char(1) == '+' || peek_char(1) == '-') ? 1 : 0;
                    if (is_digit(peek_char(1 + sign))) {
                        advance(1 + sign);
                        scan_digits(rule, is_digit);
                        is_float = true;
                    }
                }
            }

            // Reject things like `123abc` or `0xZZ` as one error token rather than splitting them.
            while (!at_end() && ident_continue(src_[pos_])) {
                advance(1);
                ok = false;
            }

            if (!ok)
                return emit(lang_.error_type());
            return emit(is_float ? rule.float_type : rule.int_type);
        }

        template <typename Pred>
        bool scan_digits(const NumberRule& rule, Pred is_valid) {
            std::size_t count = 0;
            while (!at_end()) {
                char c = src_[pos_];
                if (is_valid(c)) {
                    advance(1);
                    ++count;
                } else if (rule.digit_separator != '\0' && c == rule.digit_separator && count > 0 && is_valid(peek_char(1))) {
                    advance(1);
                } else {
                    break;
                }
            }
            return count > 0;
        }

        TokenType lex_word() {
            // The first character already passed ident_start() and may not satisfy ident_continue().
            advance(1);
            while (!at_end() && ident_continue(src_[pos_]))
                advance(1);
            std::string_view word = src_.substr(start_, pos_ - start_);

            if (const auto* kw = lang_.find_keyword(word))
                return emit(kw->type);
            // Word-shaped symbols such as `and` win over identifiers.
            if (const auto* sym = lang_.match_symbol(word); sym && sym->text.size() == word.size())
                return emit(sym->type);
            if (lang_.identifier_rule())
                return emit(lang_.identifier_rule()->type);
            return emit(lang_.error_type());
        }

        bool ident_start(char c) const {
            const auto* rule = lang_.identifier_rule();
            return rule ? rule->is_start(c) : detail::default_ident_start(c);
        }

        bool ident_continue(char c) const {
            const auto* rule = lang_.identifier_rule();
            return rule ? rule->is_continue(c) : detail::default_ident_continue(c);
        }

        static bool is_digit(char c) { return c >= '0' && c <= '9'; }
        static bool is_binary_digit(char c) { return c == '0' || c == '1'; }
        static bool is_hex_digit(char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }

        static bool is_continuation_byte(char c) {
            return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
        }

        bool at_end() const { return pos_ >= src_.size(); }
        std::string_view rest() const { return src_.substr(pos_); }
        bool starts_with(std::string_view s) const { return rest().substr(0, s.size()) == s; }

        char peek_char(std::size_t ahead) const {
            return pos_ + ahead < src_.size() ? src_[pos_ + ahead] : '\0';
        }

        void advance(std::size_t n) {
            for (; n > 0 && !at_end(); --n) {
                char c = src_[pos_++];
                if (c == '\n') {
                    ++line_;
                    column_ = 1;
                } else if (!is_continuation_byte(c)) {
                    ++column_;
                }
            }
        }

        void advance_code_point() {
            advance(1);
            while (!at_end() && is_continuation_byte(src_[pos_]))
                advance(1);
        }

        TokenType emit(TokenT type) const {
            return TokenType{type, src_.substr(start_, pos_ - start_), start_line_, start_column_, start_};
        }

        const LangDefT& lang_;
        std::string_view src_;
        std::size_t pos_ = 0;
        std::size_t line_ = 1;
        std::size_t column_ = 1;
        std::size_t start_ = 0;
        std::size_t start_line_ = 1;
        std::size_t start_column_ = 1;
        std::optional<TokenType> peeked_;
        bool finished_ = false;
};

template <typename TokenT>
std::vector<Token<TokenT>> tokenize(const LangDef<TokenT>& lang, std::string_view source) {
    return Lexer<TokenT>(lang, source).tokenize();
}

template <typename TokenT>
std::vector<Token<TokenT>> tokenize(const LangDef<TokenT>&&, std::string_view) = delete;

template <typename TokenT, typename S, typename = std::enable_if_t<std::is_same_v<S, std::string>>>
std::vector<Token<TokenT>> tokenize(const LangDef<TokenT>&, S&&) = delete;

}
