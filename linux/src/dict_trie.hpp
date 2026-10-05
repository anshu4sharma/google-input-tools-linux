#ifndef GOOGLE_INPUT_TOOLS_DICT_TRIE_HPP
#define GOOGLE_INPUT_TOOLS_DICT_TRIE_HPP

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace google_input_tools {

struct RankedCand {
    std::string text;
    uint32_t freq{0};
};

class PackedDict {
public:
    bool load_file(const std::string& path);
    bool save_file(const std::string& path) const;
    void insert(const std::string& roman_key, const std::string& cand, uint32_t freq);
    void merge_from_map(const std::unordered_map<std::string, std::vector<std::string>>& exact,
                        uint32_t base_freq);

    bool empty() const { return nodes_.size() <= 1; }
    bool has_exact(const std::string& roman_key) const;
    std::vector<std::string> exact(const std::string& roman_key, size_t max_n) const;
    std::vector<std::string> prefix(const std::string& roman_prefix, size_t max_n) const;

    static std::string find_dict_path();

private:
    struct Node {
        std::array<int32_t, 26> child{};
        std::vector<RankedCand> terms;
        Node() { child.fill(-1); }
    };

    std::vector<Node> nodes_;
    mutable std::mutex mu_;

    int32_t ensure_root();
    int32_t walk(const std::string& key, bool create);
    static int letter_index(char c);
    void collect_dfs(int32_t node, std::vector<RankedCand>& out, size_t limit) const;
};

std::string deva_to_roman(const std::string& devanagari);
std::string chromeos_transliterate(const std::string& roman);

} // namespace google_input_tools

#endif
