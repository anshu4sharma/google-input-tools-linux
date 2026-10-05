#include "dict_trie.hpp"
#include "chromeos_deva_data.hpp"
#include "devanagari_data.hpp"
#include "engine.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace google_input_tools {

static bool is_deva_consonant(uint32_t cp) {
    return (cp >= 0x0915 && cp <= 0x0939) || (cp >= 0x0958 && cp <= 0x095F);
}

static const char* vowel_to_matra(uint32_t independent) {
    switch (independent) {
        case 0x0905: return "";      // a / inherent
        case 0x0906: return "ा";
        case 0x0907: return "ि";
        case 0x0908: return "ी";
        case 0x0909: return "ु";
        case 0x090A: return "ू";
        case 0x090B: return "ृ";
        case 0x090F: return "े";
        case 0x0910: return "ै";
        case 0x0913: return "ो";
        case 0x0914: return "ौ";
        default: return nullptr;
    }
}

static uint32_t last_codepoint(const std::string& s, size_t* start_out) {
    if (s.empty()) {
        if (start_out) *start_out = 0;
        return 0;
    }
    size_t i = s.size();
    while (i > 0) {
        --i;
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) break;
    }
    size_t br = 0;
    uint32_t cp = TransliterationEngine::decode_utf8(s.data() + i, s.size() - i, &br);
    if (start_out) *start_out = i;
    return cp;
}

static bool is_simple_pattern(const char* p) {
    if (!p || !*p) return false;
    for (const char* c = p; *c; ++c) {
        unsigned char u = static_cast<unsigned char>(*c);
        if (u >= 0x80) return false;
        if (*c == '[' || *c == '(' || *c == '\\' || *c == '$' || *c == '.') return false;
    }
    return true;
}

static void apply_simple_literal_rules(std::string& buf) {
    bool changed = true;
    int guard = 0;
    while (changed && guard++ < 32) {
        changed = false;
        size_t best_len = 0;
        const char* best_rep = nullptr;
        for (size_t r = 0; r < CHROMEOS_DEVA_RULES_COUNT; ++r) {
            const char* pat = CHROMEOS_DEVA_RULES[r].pattern;
            if (!is_simple_pattern(pat)) continue;
            size_t plen = std::strlen(pat);
            if (plen == 0 || plen > buf.size() || plen < best_len) continue;
            if (buf.compare(buf.size() - plen, plen, pat) == 0) {
                best_len = plen;
                best_rep = CHROMEOS_DEVA_RULES[r].replacement;
            }
        }
        if (best_rep) {
            buf.resize(buf.size() - best_len);
            buf += best_rep;
            changed = true;
        }
    }
}

static void coalesce_vowel_and_conjunct(std::string& buf) {
    size_t last_start = 0;
    uint32_t last = last_codepoint(buf, &last_start);
    if (!last) return;

    const char* matra = vowel_to_matra(last);
    if (matra && last_start > 0) {
        size_t prev_start = 0;
        uint32_t prev = last_codepoint(buf.substr(0, last_start), &prev_start);
        (void)prev_start;
        if (is_deva_consonant(prev)) {
            buf.erase(last_start);
            buf += matra;
            return;
        }
    }

    if (is_deva_consonant(last) && last_start > 0) {
        size_t prev_start = 0;
        std::string head = buf.substr(0, last_start);
        uint32_t prev = last_codepoint(head, &prev_start);
        if (is_deva_consonant(prev)) {
            std::string cons = buf.substr(last_start);
            buf.resize(last_start);
            buf += "्";
            buf += cons;
        }
    }
}

std::string chromeos_transliterate(const std::string& roman) {
    if (roman.empty()) return "";
    std::string buf;
    buf.reserve(roman.size() * 3);
    for (char c : roman) {
        buf.push_back(c);
        apply_simple_literal_rules(buf);
        coalesce_vowel_and_conjunct(buf);
    }
    return buf;
}

static const std::unordered_map<uint32_t, const char*>& phonetic_keys() {
    static std::unordered_map<uint32_t, const char*> map;
    static bool init = false;
    if (!init) {
        for (size_t i = 0; i < DEVANAGARI_CHARS_COUNT; ++i) {
            map[DEVANAGARI_CHARS[i].codepoint] = DEVANAGARI_CHARS[i].phonetic_key;
        }
        init = true;
    }
    return map;
}

std::string deva_to_roman(const std::string& devanagari) {
    const auto& keys = phonetic_keys();
    std::string out;
    out.reserve(devanagari.size());
    size_t i = 0;
    const size_t n = devanagari.size();
    while (i < n) {
        size_t br = 0;
        uint32_t cp = TransliterationEngine::decode_utf8(devanagari.data() + i, n - i, &br);
        if (br == 0) break;
        i += br;
        if (cp == 0x094D) continue; // virama: no inherent a
        auto it = keys.find(cp);
        const char* k = it != keys.end() ? it->second : nullptr;
        if (!k) continue;

        if (cp >= 0x0915 && cp <= 0x095F) {
            out += k;
            if (i < n) {
                size_t nbr = 0;
                uint32_t next = TransliterationEngine::decode_utf8(devanagari.data() + i, n - i, &nbr);
                if (next == 0x094D) {
                    // conjunct: skip virama, no 'a'
                    continue;
                }
                if (next >= 0x093E && next <= 0x094C) {
                    continue; // matra follows
                }
                if (next == 0x093C) {
                    continue; // nukta
                }
            }
            out += 'a';
        } else {
            out += k;
        }
    }
    if (out.size() >= 2 && out.back() == 'a') {
        // Hindi schwa deletion at end of word: keep both via caller
    }
    return out;
}

} // namespace google_input_tools
