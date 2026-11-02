// Lexes a small calculator language and prints each token.
//
//   make example
//   ./build/calc 'let x = 2 * (y + 1.5) # comment'

#include "lexengine.hpp"

#include <iostream>
#include <string>

enum class Tok { Let, Ident, Number, Float, Plus, Minus, Star, Slash, Assign, LParen, RParen, Eof, Error };

const char* name(Tok t) {
    switch (t) {
        case Tok::Let: return "Let";
        case Tok::Ident: return "Ident";
        case Tok::Number: return "Number";
        case Tok::Float: return "Float";
        case Tok::Plus: return "Plus";
        case Tok::Minus: return "Minus";
        case Tok::Star: return "Star";
        case Tok::Slash: return "Slash";
        case Tok::Assign: return "Assign";
        case Tok::LParen: return "LParen";
        case Tok::RParen: return "RParen";
        case Tok::Eof: return "Eof";
        case Tok::Error: return "Error";
    }
    return "?";
}

int main(int argc, char** argv) {
    lexengine::LangDef<Tok> lang(Tok::Eof, Tok::Error);
    lang.keyword("let", Tok::Let)
        .symbol("+", Tok::Plus)
        .symbol("-", Tok::Minus)
        .symbol("*", Tok::Star)
        .symbol("/", Tok::Slash)
        .symbol("=", Tok::Assign)
        .symbol("(", Tok::LParen)
        .symbol(")", Tok::RParen)
        .identifier(Tok::Ident)
        .numbers(Tok::Number, Tok::Float)
        .line_comment("#");

    std::string source = argc > 1 ? argv[1] : "let x = 2 * (y + 1.5) # comment";

    lexengine::Lexer<Tok> lexer(lang, source);
    while (!lexer.done()) {
        auto tok = lexer.next();
        std::cout << tok.line << ":" << tok.column << "  " << name(tok.type) << "  '" << tok.text << "'\n";
    }
}
