// Including the library in a second translation unit catches non-inline
// definitions in the headers, which would fail at link time.

#include "lexengine.hpp"

#include <cstddef>
#include <string_view>

namespace {

enum class Word { Word, Eof, Error };

}

std::size_t count_words_in_second_tu(std::string_view source) {
    lexengine::LangDef<Word> lang(Word::Eof, Word::Error);
    lang.identifier(Word::Word);
    return lexengine::tokenize(lang, source).size() - 1;
}
