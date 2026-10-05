#include "dict_trie.hpp"
#include "devanagari_data.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace google_input_tools;

static void add_roman_variants(PackedDict& dict, const std::string& roman, const std::string& hindi, uint32_t freq) {
    if (roman.empty()) return;
    std::string key = roman;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    key.erase(std::remove_if(key.begin(), key.end(), [](char c) {
                  return c < 'a' || c > 'z';
              }),
              key.end());
    if (key.empty()) return;
    dict.insert(key, hindi, freq);
    if (key.size() > 2 && key.back() == 'a') {
        dict.insert(key.substr(0, key.size() - 1), hindi, freq > 5 ? freq - 5 : freq);
    }
}

static void load_hunspell(PackedDict& dict, const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "warning: cannot open hunspell dict " << path << "\n";
        return;
    }
    std::string line;
    if (!std::getline(in, line)) return; // count header
    uint32_t n = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto slash = line.find('/');
        if (slash != std::string::npos) line.resize(slash);
        // skip non-devanagari
        bool deva = false;
        for (unsigned char c : line) {
            if (c >= 0x80) {
                deva = true;
                break;
            }
        }
        if (!deva) continue;
        std::string roman = deva_to_roman(line);
        add_roman_variants(dict, roman, line, 800);
        ++n;
    }
    std::cerr << "hunspell lemmas packed: " << n << "\n";
}

static void load_tsv(PackedDict& dict, const std::string& path) {
    std::ifstream in(path);
    if (!in) return;
    std::string line;
    uint32_t n = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string roman, hindi;
        if (!std::getline(iss, roman, '\t')) continue;
        if (!std::getline(iss, hindi, '\t')) continue;
        uint32_t freq = 2000;
        std::string freq_s;
        if (std::getline(iss, freq_s, '\t') && !freq_s.empty()) {
            freq = static_cast<uint32_t>(std::strtoul(freq_s.c_str(), nullptr, 10));
        }
        add_roman_variants(dict, roman, hindi, freq);
        ++n;
    }
    std::cerr << "tsv packed " << path << ": " << n << "\n";
}

int main(int argc, char** argv) {
    std::string out_path = "assets/dict/hi_t13n.bin";
    std::string hunspell;
    std::vector<std::string> tsvs;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (a == "--hunspell" && i + 1 < argc) {
            hunspell = argv[++i];
        } else if (a.size() >= 4 && a.substr(a.size() - 4) == ".tsv") {
            tsvs.push_back(a);
        } else if (a.size() >= 4 && a.substr(a.size() - 4) == ".dic") {
            hunspell = a;
        }
    }

    PackedDict dict;
    uint32_t n_builtin = 0;
    for (size_t i = 0; i < DEVANAGARI_ENTRIES_COUNT; ++i) {
        const DictEntry& e = DEVANAGARI_ENTRIES[i];
        uint32_t f = 50000;
        if (e.cand1) {
            dict.insert(e.key, e.cand1, f);
            ++n_builtin;
        }
        if (e.cand2) dict.insert(e.key, e.cand2, f - 20);
        if (e.cand3) dict.insert(e.key, e.cand3, f - 40);
    }
    std::cerr << "builtin roman entries: " << n_builtin << "\n";

    if (!hunspell.empty()) load_hunspell(dict, hunspell);
    for (const auto& t : tsvs) load_tsv(dict, t);

    if (!dict.save_file(out_path)) {
        std::cerr << "failed to write " << out_path << "\n";
        return 1;
    }
    std::cerr << "wrote " << out_path << "\n";
    return 0;
}
