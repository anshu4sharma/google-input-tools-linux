#ifndef GOOGLE_INPUT_TOOLS_ENGINE_HPP
#define GOOGLE_INPUT_TOOLS_ENGINE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <memory>

namespace google_input_tools {

class PackedDict;

class TransliterationEngine {
public:
    explicit TransliterationEngine(const std::string& lang_code = "hi");
    ~TransliterationEngine() = default;

    void set_language(const std::string& lang_code);
    const std::string& get_language() const { return lang_code_; }

    std::vector<std::string> get_candidates(const std::string& word, size_t max_candidates = 5);
    std::string transliterate_word(const std::string& word);
    std::string transliterate_text(const std::string& text);

    void remember_choice(const std::string& roman_word, const std::string& chosen);

    static bool remove_last_grapheme(std::string& text);
    static bool remove_last_word(std::string& text);

    static bool is_combining_mark(uint32_t cp);
    static uint32_t decode_utf8(const char* s, size_t len, size_t* bytes_read);

private:
    std::string lang_code_;
    std::unordered_map<std::string, std::vector<std::string>> lru_cache_;
    size_t max_cache_size_{10000};
    std::string disk_cache_path_;

    void load_disk_cache();
    void save_to_disk_cache(const std::string& word, const std::vector<std::string>& candidates);
    std::string rule_based_transliterate(const std::string& word, int variation = 0);
    void add_unique(std::vector<std::string>& out, const std::string& cand, size_t max_n) const;
};

} // namespace google_input_tools

extern "C" {
    void* git_engine_new(const char* lang);
    void git_engine_free(void* engine);
    int git_get_candidates(void* engine, const char* word, char** out_candidates, int max_candidates, int max_len);
    int git_transliterate_text(void* engine, const char* text, char* out_buf, int out_len);
    int git_remove_last_grapheme(char* text);
    int git_remove_last_word(char* text);
}

#endif
