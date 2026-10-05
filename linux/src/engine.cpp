#include "engine.hpp"
#include "devanagari_data.hpp"
#include "dict_trie.hpp"
#include "emergency_api.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <iostream>
#include <fstream>
#include <array>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>

namespace google_input_tools {

// Local user customization and frequency cache
static std::string get_cache_file_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    std::string dir = std::string(home) + "/.cache/google-input-tools";
    mkdir(dir.c_str(), 0755);
    return dir + "/user_history.txt";
}

static std::mutex g_cache_mutex;
static std::mutex g_overlay_mutex;
static std::unordered_map<std::string, std::vector<std::string>> g_emergency_overlay;

static PackedDict& global_packed_dict() {
    static PackedDict dict;
    static bool loaded = false;
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    if (!loaded) {
        loaded = true;
        std::string path = PackedDict::find_dict_path();
        if (!path.empty()) {
            dict.load_file(path);
        }
    }
    return dict;
}

static void apply_emergency_result(const std::string& key, const std::vector<std::string>& cands) {
    if (cands.empty()) return;
    {
        std::lock_guard<std::mutex> lock(g_overlay_mutex);
        g_emergency_overlay[key] = cands;
    }
    for (size_t i = 0; i < cands.size(); ++i) {
        global_packed_dict().insert(key, cands[i], 150000u - static_cast<uint32_t>(i) * 10u);
    }
    std::string path;
    {
        const char* home = std::getenv("HOME");
        if (home) path = std::string(home) + "/.cache/google-input-tools/user_history.txt";
    }
    if (path.empty()) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    std::ofstream outfile(path, std::ios::app);
    if (!outfile.is_open()) return;
    outfile << key;
    for (const auto& c : cands) outfile << " " << c;
    outfile << "\n";
}

TransliterationEngine::TransliterationEngine(const std::string& lang_code)
    : lang_code_(lang_code) {
    disk_cache_path_ = get_cache_file_path();
    global_packed_dict();
    load_disk_cache();
}

void TransliterationEngine::add_unique(std::vector<std::string>& out, const std::string& cand, size_t max_n) const {
    if (cand.empty() || out.size() >= max_n) return;
    if (std::find(out.begin(), out.end(), cand) != out.end()) return;
    out.push_back(cand);
}

void TransliterationEngine::remember_choice(const std::string& roman_word, const std::string& chosen) {
    if (roman_word.empty() || chosen.empty()) return;
    std::string key = roman_word;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::string> cands;
    cands.push_back(chosen);
    auto it = lru_cache_.find(key);
    if (it != lru_cache_.end()) {
        for (const auto& p : it->second) {
            if (p != chosen) cands.push_back(p);
            if (cands.size() >= 5) break;
        }
    }
    lru_cache_[key] = cands;
    save_to_disk_cache(key, cands);
    global_packed_dict().insert(key, chosen, 200000);
}

void TransliterationEngine::set_language(const std::string& lang_code) {
    lang_code_ = lang_code;
    lru_cache_.clear();
}

void TransliterationEngine::load_disk_cache() {
    if (disk_cache_path_.empty()) return;
    std::ifstream infile(disk_cache_path_);
    if (!infile.is_open()) return;

    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string word;
        if (!(iss >> word)) continue;

        std::vector<std::string> cands;
        std::string cand;
        while (iss >> cand) {
            cands.push_back(cand);
        }
        if (!cands.empty()) {
            lru_cache_[word] = cands;
        }
    }
}

void TransliterationEngine::save_to_disk_cache(const std::string& word, const std::vector<std::string>& candidates) {
    if (disk_cache_path_.empty() || candidates.empty()) return;
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    std::ofstream outfile(disk_cache_path_, std::ios::app);
    if (!outfile.is_open()) return;

    outfile << word;
    for (const auto& c : candidates) {
        outfile << " " << c;
    }
    outfile << "\n";
}

// Complete Devanagari Hindi Phonetic Transliteration Engine
struct ConsonantMapping {
    const char* pattern;
    const char* deva;
};

static const ConsonantMapping CONSONANTS[] = {
    {"kshy", "क्ष्य"}, {"ksh", "क्ष"}, {"x", "क्ष"},
    {"jny", "ज्ञ"}, {"gy", "ज्ञ"},
    {"shr", "श्र"},
    {"tr", "त्र"},
    {"chh", "छ"}, {"Ch", "छ"}, {"ch", "च"}, {"c", "च"},
    {"kh.", "ख़"}, {"q", "क़"}, {"k.", "क़"}, {"kh", "ख"}, {"k", "क"},
    {"gh", "घ"}, {"g.", "ग़"}, {"g", "ग"}, {"ng", "ङ"},
    {"jh", "झ"}, {"z", "ज़"}, {"j.", "ज़"}, {"j", "ज"},
    {"Tth", "ठ"}, {"Th", "ठ"}, {"th", "थ"},
    {"Tt", "ट"}, {"T", "ट"}, {"t", "त"},
    {"Ddh", "ढ"}, {"Dh.", "ढ़"}, {"Dh", "ढ"}, {"dh", "ध"},
    {"Dd", "ड"}, {"D.", "ड़"}, {"D", "ड"}, {"d", "द"},
    {"N", "ण"}, {"n", "न"},
    {"ph", "फ"}, {"f.", "फ़"}, {"f", "फ़"}, {"p", "प"},
    {"bh", "भ"}, {"b", "ब"},
    {"m", "म"},
    {"y", "य"}, {"r", "र"}, {"l", "ल"}, {"v", "व"}, {"w", "व"},
    {"shh", "ष"}, {"Sh", "ष"}, {"sh", "श"},
    {"s", "स"}, {"h", "ह"}, {"L", "ळ"}
};

struct VowelMapping {
    const char* pattern;
    const char* independent;
    const char* matra;
};

static const VowelMapping VOWELS[] = {
    {"ain", "ऐं", "ैं"},
    {"aen", "ऐं", "ैं"},
    {"ein", "एं", "ें"},
    {"aa", "आ", "ा"},
    {"A", "आ", "ा"},
    {"ee", "ई", "ी"},
    {"ii", "ई", "ी"},
    {"I", "ई", "ी"},
    {"oo", "ऊ", "ू"},
    {"uu", "ऊ", "ू"},
    {"U", "ऊ", "ू"},
    {"ai", "ऐ", "ै"},
    {"ae", "ऐ", "ै"},
    {"au", "औ", "ौ"},
    {"ou", "औ", "ौ"},
    {"ri", "ऋ", "ृ"},
    {"R", "ऋ", "ृ"},
    {"e", "ए", "े"},
    {"E", "ए", "े"},
    {"o", "ओ", "ो"},
    {"O", "ओ", "ो"},
    {"i", "इ", "ि"},
    {"u", "उ", "ु"},
    {"a", "अ", ""}
};

std::string TransliterationEngine::rule_based_transliterate(const std::string& input, int variation) {
    if (input.empty()) return "";
    std::string out;
    size_t i = 0;
    size_t n = input.size();
    bool last_was_consonant = false;

    while (i < n) {
        // Nasal anusvara check: 'n' before consonant (e.g. "nd", "nt", "nk", "st"...)
        if (last_was_consonant && input[i] == 'n' && i + 1 < n) {
            char next = input[i + 1];
            if (next != 'a' && next != 'e' && next != 'i' && next != 'o' && next != 'u') {
                out += "ं";
                i++;
                last_was_consonant = false;
                continue;
            }
        }

        // Match vowels (longest first)
        bool matched_vowel = false;
        for (const auto& vi : VOWELS) {
            size_t plen = std::strlen(vi.pattern);
            if (i + plen <= n && input.compare(i, plen, vi.pattern) == 0) {
                if (last_was_consonant) {
                    if (out.size() >= 3 && out.substr(out.size() - 3) == "\u094D") {
                        out.erase(out.size() - 3);
                    }
                    if (std::strcmp(vi.pattern, "a") == 0) {
                        // Variation 1: final 'a' becomes long 'aa' matra "ा"
                        if (i + plen == n && variation == 1) {
                            out += "ा";
                        }
                    } else if (std::strcmp(vi.pattern, "i") == 0 && variation == 2) {
                        out += "ी"; // alternate long 'ee'
                    } else if (std::strcmp(vi.pattern, "u") == 0 && variation == 2) {
                        out += "ू"; // alternate long 'oo'
                    } else {
                        out += vi.matra;
                    }
                } else {
                    out += vi.independent;
                }
                i += plen;
                last_was_consonant = false;
                matched_vowel = true;
                break;
            }
        }
        if (matched_vowel) continue;

        // Match consonants (longest first)
        bool matched_consonant = false;
        for (const auto& ci : CONSONANTS) {
            size_t plen = std::strlen(ci.pattern);
            if (i + plen <= n && input.compare(i, plen, ci.pattern) == 0) {
                out += ci.deva;
                out += "\u094D"; // halant by default
                i += plen;
                last_was_consonant = true;
                matched_consonant = true;
                break;
            }
        }
        if (matched_consonant) continue;

        // Non-alphabetic symbols, digits, punctuation
        if (last_was_consonant) {
            if (out.size() >= 3 && out.substr(out.size() - 3) == "\u094D") {
                out.erase(out.size() - 3);
            }
            last_was_consonant = false;
        }

        char c = input[i];
        if (c >= '0' && c <= '9') {
            const char* deva_digits[] = {"०","१","२","३","४","५","६","७","८","९"};
            out += deva_digits[c - '0'];
        } else if (c == '|') {
            out += "।";
        } else {
            out += c;
        }
        i++;
    }

    // Modern Hindi Schwa deletion: remove trailing halant at end of word
    if (out.size() >= 3 && out.substr(out.size() - 3) == "\u094D") {
        out.erase(out.size() - 3);
    }

    return out;
}

std::vector<std::string> TransliterationEngine::get_candidates(const std::string& raw_word, size_t max_candidates) {
    std::string word = raw_word;
    auto b = word.find_first_not_of(" \t\n\r");
    if (b == std::string::npos) return {};
    word.erase(0, b);
    auto e = word.find_last_not_of(" \t\n\r");
    if (e != std::string::npos) word.erase(e + 1);

    if (word.empty()) return {};

    std::string lower_word = word;
    std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    {
        std::lock_guard<std::mutex> lock(g_overlay_mutex);
        auto ov = g_emergency_overlay.find(lower_word);
        if (ov != g_emergency_overlay.end()) {
            return ov->second;
        }
    }

    auto cache_it = lru_cache_.find(lower_word);
    if (cache_it != lru_cache_.end()) {
        return cache_it->second;
    }

    std::vector<std::string> candidates;
    PackedDict& packed = global_packed_dict();
    bool exact_hit = packed.has_exact(lower_word);

    for (const auto& c : packed.exact(lower_word, max_candidates)) {
        add_unique(candidates, c, max_candidates);
    }

    const auto& deva_dict = DevanagariDict::get_map();
    auto dict_it = deva_dict.find(lower_word);
    if (dict_it != deva_dict.end()) {
        exact_hit = true;
        for (const auto& c : dict_it->second) {
            add_unique(candidates, c, max_candidates);
        }
    }

    for (const auto& c : packed.prefix(lower_word, max_candidates * 3)) {
        add_unique(candidates, c, max_candidates);
    }

    add_unique(candidates, chromeos_transliterate(lower_word), max_candidates);
    add_unique(candidates, rule_based_transliterate(lower_word, 1), max_candidates);
    add_unique(candidates, rule_based_transliterate(lower_word, 0), max_candidates);
    add_unique(candidates, rule_based_transliterate(lower_word, 2), max_candidates);
    add_unique(candidates, word, max_candidates);

    if (lru_cache_.size() >= max_cache_size_) {
        lru_cache_.erase(lru_cache_.begin());
    }
    lru_cache_[lower_word] = candidates;

    maybe_request_emergency_candidates(lower_word, exact_hit, apply_emergency_result);

    return candidates;
}

std::string TransliterationEngine::transliterate_word(const std::string& word) {
    auto cands = get_candidates(word, 1);
    return cands.empty() ? word : cands[0];
}

std::string TransliterationEngine::transliterate_text(const std::string& text) {
    if (text.empty()) return "";

    std::string result;
    std::string current_word;

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (std::isalpha(static_cast<unsigned char>(c))) {
            current_word += c;
        } else {
            if (!current_word.empty()) {
                result += transliterate_word(current_word);
                current_word.clear();
            }
            result += c;
        }
    }

    if (!current_word.empty()) {
        result += transliterate_word(current_word);
    }

    return result;
}

uint32_t TransliterationEngine::decode_utf8(const char* s, size_t len, size_t* bytes_read) {
    if (!s || len == 0) {
        if (bytes_read) *bytes_read = 0;
        return 0;
    }
    unsigned char c = static_cast<unsigned char>(s[0]);
    if (c < 0x80) {
        if (bytes_read) *bytes_read = 1;
        return c;
    } else if ((c & 0xE0) == 0xC0) {
        if (len < 2) { if (bytes_read) *bytes_read = len; return 0; }
        if (bytes_read) *bytes_read = 2;
        return ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[1]) & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        if (len < 3) { if (bytes_read) *bytes_read = len; return 0; }
        if (bytes_read) *bytes_read = 3;
        return ((c & 0x0F) << 12) |
               ((static_cast<unsigned char>(s[1]) & 0x3F) << 6) |
               (static_cast<unsigned char>(s[2]) & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        if (len < 4) { if (bytes_read) *bytes_read = len; return 0; }
        if (bytes_read) *bytes_read = 4;
        return ((c & 0x07) << 18) |
               ((static_cast<unsigned char>(s[1]) & 0x3F) << 12) |
               ((static_cast<unsigned char>(s[2]) & 0x3F) << 6) |
               (static_cast<unsigned char>(s[3]) & 0x3F);
    }
    if (bytes_read) *bytes_read = 1;
    return c;
}

bool TransliterationEngine::is_combining_mark(uint32_t cp) {
    if (cp >= 0x0901 && cp <= 0x0903) return true;
    if (cp == 0x093C) return true;
    if (cp >= 0x093E && cp <= 0x094D) return true;
    if (cp >= 0x0951 && cp <= 0x0957) return true;
    if (cp == 0x0962 || cp == 0x0963) return true;
    return false;
}

bool TransliterationEngine::remove_last_grapheme(std::string& text) {
    if (text.empty()) return false;

    size_t len = text.size();
    size_t i = len;

    while (i > 0) {
        --i;
        if ((static_cast<unsigned char>(text[i]) & 0xC0) != 0x80) {
            break;
        }
    }

    size_t bytes_read = 0;
    uint32_t last_cp = decode_utf8(text.data() + i, len - i, &bytes_read);

    while (is_combining_mark(last_cp) && i > 0) {
        size_t prev_i = i;
        while (prev_i > 0) {
            --prev_i;
            if ((static_cast<unsigned char>(text[prev_i]) & 0xC0) != 0x80) {
                break;
            }
        }
        last_cp = decode_utf8(text.data() + prev_i, i - prev_i, &bytes_read);
        i = prev_i;
    }

    text.erase(i);
    return true;
}

bool TransliterationEngine::remove_last_word(std::string& text) {
    if (text.empty()) return false;

    int end = static_cast<int>(text.size()) - 1;
    while (end >= 0 && (std::isspace(static_cast<unsigned char>(text[end])) ||
                        std::ispunct(static_cast<unsigned char>(text[end])))) {
        --end;
    }

    if (end < 0) {
        text.clear();
        return true;
    }

    int start = end;
    while (start >= 0) {
        unsigned char c = static_cast<unsigned char>(text[start]);
        if (std::isspace(c)) {
            break;
        }
        if (c < 0x80 && std::ispunct(c)) {
            break;
        }
        --start;
    }

    text.erase(start + 1);
    return true;
}

} // namespace google_input_tools

extern "C" {

void* git_engine_new(const char* lang) {
    return new google_input_tools::TransliterationEngine(lang ? lang : "hi");
}

void git_engine_free(void* engine) {
    delete static_cast<google_input_tools::TransliterationEngine*>(engine);
}

int git_get_candidates(void* engine, const char* word, char** out_candidates, int max_candidates, int max_len) {
    if (!engine || !word || !out_candidates || max_candidates <= 0 || max_len <= 0) return 0;
    auto* eng = static_cast<google_input_tools::TransliterationEngine*>(engine);
    auto cands = eng->get_candidates(word, max_candidates);
    int count = 0;
    for (size_t i = 0; i < cands.size() && count < max_candidates; ++i) {
        std::strncpy(out_candidates[count], cands[i].c_str(), max_len - 1);
        out_candidates[count][max_len - 1] = '\0';
        count++;
    }
    return count;
}

int git_transliterate_text(void* engine, const char* text, char* out_buf, int out_len) {
    if (!engine || !text || !out_buf || out_len <= 0) return 0;
    auto* eng = static_cast<google_input_tools::TransliterationEngine*>(engine);
    std::string out = eng->transliterate_text(text);
    std::strncpy(out_buf, out.c_str(), out_len - 1);
    out_buf[out_len - 1] = '\0';
    return static_cast<int>(out.size());
}

int git_remove_last_grapheme(char* text) {
    if (!text) return 0;
    std::string s(text);
    bool ok = google_input_tools::TransliterationEngine::remove_last_grapheme(s);
    if (ok) {
        std::strcpy(text, s.c_str());
        return 1;
    }
    return 0;
}

int git_remove_last_word(char* text) {
    if (!text) return 0;
    std::string s(text);
    bool ok = google_input_tools::TransliterationEngine::remove_last_word(s);
    if (ok) {
        std::strcpy(text, s.c_str());
        return 1;
    }
    return 0;
}

}
