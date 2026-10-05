#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace google_input_tools {

enum class DevaCharType {
    Vowel,
    Consonant,
    Matra,
    Sign,
    Digit,
    Punctuation
};

struct DevaCharInfo {
    uint32_t codepoint;
    const char* utf8;
    const char* name;
    DevaCharType type;
    const char* phonetic_key;
};

extern const DevaCharInfo DEVANAGARI_CHARS[];
extern const size_t DEVANAGARI_CHARS_COUNT;

struct DictEntry {
    const char* key;
    const char* cand1;
    const char* cand2;
    const char* cand3;
};

extern const DictEntry DEVANAGARI_ENTRIES[];
extern const size_t DEVANAGARI_ENTRIES_COUNT;

struct DevanagariDict {
    static const std::unordered_map<std::string, std::vector<std::string>>& get_map();
};

} // namespace google_input_tools
