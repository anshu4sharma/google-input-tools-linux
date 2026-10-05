#include "dict_trie.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <unistd.h>
#include <linux/limits.h>

namespace google_input_tools {

static constexpr char kMagic[4] = {'G', 'I', 'T', '1'};
static constexpr uint32_t kVersion = 1;

int PackedDict::letter_index(char c) {
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c >= 'A' && c <= 'Z') return c - 'A';
    return -1;
}

int32_t PackedDict::ensure_root() {
    if (nodes_.empty()) {
        nodes_.emplace_back();
    }
    return 0;
}

int32_t PackedDict::walk(const std::string& key, bool create) {
    int32_t node = ensure_root();
    for (char ch : key) {
        int idx = letter_index(ch);
        if (idx < 0) return -1;
        int32_t next = nodes_[static_cast<size_t>(node)].child[static_cast<size_t>(idx)];
        if (next < 0) {
            if (!create) return -1;
            next = static_cast<int32_t>(nodes_.size());
            nodes_[static_cast<size_t>(node)].child[static_cast<size_t>(idx)] = next;
            nodes_.emplace_back();
        }
        node = next;
    }
    return node;
}

void PackedDict::insert(const std::string& roman_key, const std::string& cand, uint32_t freq) {
    if (roman_key.empty() || cand.empty()) return;
    std::lock_guard<std::mutex> lock(mu_);
    int32_t node = walk(roman_key, true);
    if (node < 0) return;
    auto& terms = nodes_[static_cast<size_t>(node)].terms;
    for (auto& t : terms) {
        if (t.text == cand) {
            if (freq > t.freq) t.freq = freq;
            return;
        }
    }
    terms.push_back({cand, freq});
}

void PackedDict::merge_from_map(const std::unordered_map<std::string, std::vector<std::string>>& exact,
                               uint32_t base_freq) {
    for (const auto& kv : exact) {
        uint32_t f = base_freq;
        for (const auto& c : kv.second) {
            insert(kv.first, c, f);
            if (f > 10) f -= 10;
        }
    }
}

bool PackedDict::has_exact(const std::string& roman_key) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (nodes_.empty()) return false;
    int32_t node = const_cast<PackedDict*>(this)->walk(roman_key, false);
    if (node < 0) return false;
    return !nodes_[static_cast<size_t>(node)].terms.empty();
}

std::vector<std::string> PackedDict::exact(const std::string& roman_key, size_t max_n) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> out;
    if (nodes_.empty()) return out;
    int32_t node = const_cast<PackedDict*>(this)->walk(roman_key, false);
    if (node < 0) return out;
    auto terms = nodes_[static_cast<size_t>(node)].terms;
    std::sort(terms.begin(), terms.end(), [](const RankedCand& a, const RankedCand& b) {
        return a.freq > b.freq;
    });
    for (const auto& t : terms) {
        if (out.size() >= max_n) break;
        out.push_back(t.text);
    }
    return out;
}

void PackedDict::collect_dfs(int32_t node, std::vector<RankedCand>& out, size_t limit) const {
    if (node < 0 || out.size() >= limit) return;
    for (const auto& t : nodes_[static_cast<size_t>(node)].terms) {
        out.push_back(t);
        if (out.size() >= limit) return;
    }
    const auto& ch = nodes_[static_cast<size_t>(node)].child;
    for (int i = 0; i < 26; ++i) {
        if (ch[static_cast<size_t>(i)] >= 0) {
            collect_dfs(ch[static_cast<size_t>(i)], out, limit);
            if (out.size() >= limit) return;
        }
    }
}


std::vector<std::string> PackedDict::prefix(const std::string& roman_prefix, size_t max_n) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> out;
    if (nodes_.empty() || roman_prefix.empty()) return out;
    int32_t node = const_cast<PackedDict*>(this)->walk(roman_prefix, false);
    if (node < 0) return out;

    std::vector<RankedCand> pool;
    collect_dfs(node, pool, 2500);
    std::sort(pool.begin(), pool.end(), [](const RankedCand& a, const RankedCand& b) {
        return a.freq > b.freq;
    });
    for (const auto& t : pool) {
        if (std::find(out.begin(), out.end(), t.text) != out.end()) continue;
        out.push_back(t.text);
        if (out.size() >= max_n) break;
    }
    return out;
}

bool PackedDict::save_file(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mu_);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(kMagic, 4);
    uint32_t ver = kVersion;
    out.write(reinterpret_cast<const char*>(&ver), 4);

    struct Flat {
        std::string key;
        std::vector<RankedCand> terms;
    };
    std::vector<Flat> flats;

    struct Frame {
        int32_t node;
        std::string key;
    };
    std::vector<Frame> stack;
    if (!nodes_.empty()) stack.push_back({0, ""});
    while (!stack.empty()) {
        Frame f = stack.back();
        stack.pop_back();
        if (!nodes_[static_cast<size_t>(f.node)].terms.empty()) {
            flats.push_back({f.key, nodes_[static_cast<size_t>(f.node)].terms});
        }
        for (int i = 25; i >= 0; --i) {
            int32_t nxt = nodes_[static_cast<size_t>(f.node)].child[static_cast<size_t>(i)];
            if (nxt >= 0) {
                stack.push_back({nxt, f.key + static_cast<char>('a' + i)});
            }
        }
    }

    uint32_t count = static_cast<uint32_t>(flats.size());
    out.write(reinterpret_cast<const char*>(&count), 4);
    for (const auto& e : flats) {
        uint16_t klen = static_cast<uint16_t>(e.key.size());
        out.write(reinterpret_cast<const char*>(&klen), 2);
        out.write(e.key.data(), klen);
        uint8_t nc = static_cast<uint8_t>(std::min<size_t>(e.terms.size(), 255));
        out.write(reinterpret_cast<const char*>(&nc), 1);
        for (uint8_t i = 0; i < nc; ++i) {
            uint16_t clen = static_cast<uint16_t>(e.terms[i].text.size());
            out.write(reinterpret_cast<const char*>(&clen), 2);
            out.write(e.terms[i].text.data(), clen);
            out.write(reinterpret_cast<const char*>(&e.terms[i].freq), 4);
        }
    }
    return static_cast<bool>(out);
}

bool PackedDict::load_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char mag[4];
    in.read(mag, 4);
    if (!in || std::memcmp(mag, kMagic, 4) != 0) return false;
    uint32_t ver = 0;
    in.read(reinterpret_cast<char*>(&ver), 4);
    if (ver != kVersion) return false;
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), 4);
    {
        std::lock_guard<std::mutex> lock(mu_);
        nodes_.clear();
        ensure_root();
        nodes_.reserve(std::min<size_t>(count * 2, 4000000));

        for (uint32_t i = 0; i < count; ++i) {
            uint16_t klen = 0;
            in.read(reinterpret_cast<char*>(&klen), 2);
            std::string key(klen, '\0');
            in.read(&key[0], klen);
            uint8_t nc = 0;
            in.read(reinterpret_cast<char*>(&nc), 1);
            int32_t node = walk(key, true);
            if (node >= 0) {
                auto& terms = nodes_[static_cast<size_t>(node)].terms;
                terms.reserve(terms.size() + nc);
                for (uint8_t c = 0; c < nc; ++c) {
                    uint16_t clen = 0;
                    in.read(reinterpret_cast<char*>(&clen), 2);
                    std::string cand(clen, '\0');
                    in.read(&cand[0], clen);
                    uint32_t freq = 0;
                    in.read(reinterpret_cast<char*>(&freq), 4);
                    terms.push_back({std::move(cand), freq});
                }
            } else {
                for (uint8_t c = 0; c < nc; ++c) {
                    uint16_t clen = 0;
                    in.read(reinterpret_cast<char*>(&clen), 2);
                    in.seekg(clen + 4, std::ios::cur);
                }
            }
            if (!in) return false;
        }
    }
    return true;
}

std::string PackedDict::find_dict_path() {
    const char* env = std::getenv("GIT_DICT_PATH");
    if (env && env[0] && access(env, R_OK) == 0) return env;

    const char* homes[] = {
        "/usr/share/google-input-tools/dict/hi_t13n.bin",
        "/usr/local/share/google-input-tools/dict/hi_t13n.bin",
    };
    for (const char* p : homes) {
        if (access(p, R_OK) == 0) return p;
    }

    char exe[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = 0;
        std::string dir(exe);
        auto slash = dir.find_last_of('/');
        if (slash != std::string::npos) dir.resize(slash);
        const std::string rels[] = {
            dir + "/../assets/dict/hi_t13n.bin",
            dir + "/../../assets/dict/hi_t13n.bin",
            dir + "/assets/dict/hi_t13n.bin",
        };
        for (const auto& p : rels) {
            if (access(p.c_str(), R_OK) == 0) return p;
        }
    }
    if (access("assets/dict/hi_t13n.bin", R_OK) == 0) return "assets/dict/hi_t13n.bin";
    if (access("linux/assets/dict/hi_t13n.bin", R_OK) == 0) return "linux/assets/dict/hi_t13n.bin";
    return "";
}

} // namespace google_input_tools
