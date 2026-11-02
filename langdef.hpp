#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lexengine {

namespace detail {

inline bool default_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

inline bool default_ident_continue(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

}

template <typename TokenT>
class LangDef {
    static_assert(std::is_enum_v<TokenT>, "LangDef<TokenT>: TokenT must be an enum");
    public:
        struct SymbolInfo {
            TokenT type;
            std::string text;
            std::string name;
        };

        struct IdentifierRule {
            TokenT type;
            std::function<bool(char)> is_start = detail::default_ident_start;
            std::function<bool(char)> is_continue = detail::default_ident_continue;
        };

        struct NumberRule {
            TokenT int_type;
            TokenT float_type;
            bool allow_float = true;
            bool allow_exponent = true;
            bool allow_hex = true;
            bool allow_binary = false;
            char digit_separator = '\0';
        };

        struct StringRule {
            TokenT type;
            std::string open;
            std::string close;
            char escape = '\\';
            bool multiline = false;
        };

        struct CommentRule {
            std::string open;
            std::string close; // empty = runs to end of line
            bool nestable = false;
            std::optional<TokenT> type = std::nullopt; // empty = comment is skipped
        };

        LangDef(TokenT eof, TokenT error) : eof_(eof), error_(error) {}

        LangDef& keyword(std::string text, TokenT type, std::string name = {}) {
            if (text.empty())
                throw std::invalid_argument("LangDef: keyword text is empty");
            if (!case_sensitive_)
                text = to_lower(text);
            if (name.empty())
                name = text;
            std::string key = text;
            if (!keywords_.emplace(std::move(key), SymbolInfo{type, std::move(text), std::move(name)}).second)
                throw std::invalid_argument("LangDef: duplicate keyword");
            return *this;
        }

        LangDef& symbol(std::string text, TokenT type, std::string name = {}) {
            if (text.empty())
                throw std::invalid_argument("LangDef: symbol text is empty");
            if (name.empty())
                name = text;
            std::size_t len = text.size();
            std::string key = text;
            if (!symbols_.emplace(std::move(key), SymbolInfo{type, std::move(text), std::move(name)}).second)
                throw std::invalid_argument("LangDef: duplicate symbol");
            max_symbol_len_ = std::max(max_symbol_len_, len);
            return *this;
        }

        LangDef& identifier(TokenT type) {
            IdentifierRule rule{type};
            return identifier(std::move(rule));
        }

        LangDef& identifier(IdentifierRule rule) {
            if (!rule.is_start || !rule.is_continue)
                throw std::invalid_argument("LangDef: identifier rule has an empty predicate");
            identifier_ = std::move(rule);
            has_identifier_ = true;
            return *this;
        }

        LangDef& numbers(TokenT int_type, TokenT float_type) {
            number_ = NumberRule{int_type, float_type};
            has_number_ = true;
            return *this;
        }

        LangDef& numbers(NumberRule rule) {
            number_ = rule;
            has_number_ = true;
            return *this;
        }

        LangDef& string_literal(TokenT type, std::string open, std::string close) {
            StringRule rule{type, std::move(open), std::move(close)};
            return string_literal(std::move(rule));
        }

        LangDef& string_literal(StringRule rule) {
            if (rule.open.empty() || rule.close.empty())
                throw std::invalid_argument("LangDef: string delimiter is empty");
            strings_.push_back(std::move(rule));
            return *this;
        }

        LangDef& line_comment(std::string open) {
            CommentRule rule{std::move(open), {}};
            return comment(std::move(rule));
        }

        LangDef& block_comment(std::string open, std::string close, bool nestable = false) {
            if (close.empty())
                throw std::invalid_argument("LangDef: block comment close is empty");
            CommentRule rule{std::move(open), std::move(close), nestable};
            return comment(std::move(rule));
        }

        LangDef& comment(CommentRule rule) {
            if (rule.open.empty())
                throw std::invalid_argument("LangDef: comment open is empty");
            if (rule.nestable && rule.open == rule.close)
                throw std::invalid_argument("LangDef: nestable comment needs distinct open and close");
            comments_.push_back(std::move(rule));
            return *this;
        }

        LangDef& newline(TokenT type) {
            newline_ = type;
            return *this;
        }

        // Must be called before any keyword is added.
        LangDef& case_insensitive_keywords() {
            if (!keywords_.empty())
                throw std::logic_error("LangDef: case_insensitive_keywords() called after keyword()");
            case_sensitive_ = false;
            return *this;
        }

        const SymbolInfo* find_keyword(std::string_view word) const {
            auto it = case_sensitive_ ? keywords_.find(word) : keywords_.find(to_lower(word));
            return it == keywords_.end() ? nullptr : &it->second;
        }

        // Longest symbol that is a prefix of rest.
        const SymbolInfo* match_symbol(std::string_view rest) const {
            for (std::size_t len = std::min(max_symbol_len_, rest.size()); len > 0; --len) {
                auto it = symbols_.find(rest.substr(0, len));
                if (it != symbols_.end())
                    return &it->second;
            }
            return nullptr;
        }

        const CommentRule* match_comment(std::string_view rest) const {
            return longest_open(comments_, rest);
        }

        const StringRule* match_string(std::string_view rest) const {
            return longest_open(strings_, rest);
        }

        // Null when the language has no such rule.
        const IdentifierRule* identifier_rule() const { return has_identifier_ ? &identifier_ : nullptr; }
        const NumberRule* number_rule() const { return has_number_ ? &number_ : nullptr; }
        const std::vector<StringRule>& string_rules() const { return strings_; }
        const std::vector<CommentRule>& comment_rules() const { return comments_; }
        const std::optional<TokenT>& newline_type() const { return newline_; }
        TokenT eof_type() const { return eof_; }
        TokenT error_type() const { return error_; }
        bool case_sensitive() const { return case_sensitive_; }

    private:
        static std::string to_lower(std::string_view text) {
            std::string out(text);
            for (char& c : out)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return out;
        }

        template <typename RuleT>
        static const RuleT* longest_open(const std::vector<RuleT>& rules, std::string_view rest) {
            const RuleT* best = nullptr;
            for (const RuleT& rule : rules) {
                if (rest.substr(0, rule.open.size()) != rule.open)
                    continue;
                if (!best || rule.open.size() > best->open.size())
                    best = &rule;
            }
            return best;
        }

        TokenT eof_;
        TokenT error_;
        std::optional<TokenT> newline_;
        bool case_sensitive_ = true;

        std::map<std::string, SymbolInfo, std::less<>> keywords_;
        std::map<std::string, SymbolInfo, std::less<>> symbols_;
        std::size_t max_symbol_len_ = 0;

        // Not std::optional: GCC gives false -Wmaybe-uninitialized warnings for optional<std::function> under ASan.
        IdentifierRule identifier_{};
        bool has_identifier_ = false;
        NumberRule number_{};
        bool has_number_ = false;
        std::vector<StringRule> strings_;
        std::vector<CommentRule> comments_;
};

}
