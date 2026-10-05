#include "engine.hpp"
#include <iostream>
#include <chrono>
#include <cassert>

int main() {
    using namespace google_input_tools;
    TransliterationEngine engine("hi");

    std::cout << "=== Benchmarking C++ Transliteration Engine ===" << std::endl;

    // 1. Correctness checks
    assert(engine.transliterate_word("namaste") == "नमस्ते");
    assert(engine.transliterate_word("bharat") == "भारत");
    assert(engine.transliterate_word("kya") == "क्या");
    assert(engine.transliterate_word("dost") == "दोस्त");
    assert(engine.transliterate_word("aap") == "आप");
    assert(engine.transliterate_word("kaise") == "कैसे");
    assert(engine.transliterate_word("ho") == "हो");
    assert(engine.transliterate_word("duniya") == "दुनिया");
    assert(engine.transliterate_word("dhanyavad") == "धन्यवाद");
    assert(engine.transliterate_word("mera") == "मेरा");
    assert(engine.transliterate_word("khana") == "खाना");
    assert(engine.transliterate_word("peena") == "पीना");

    std::cout << "  ✓ Core Hindi words: PASSED" << std::endl;

    // 2. Sentence transliteration
    std::string s = "namaste dosto! aap kaise ho? mera bharat mahan hai.";
    std::string out = engine.transliterate_text(s);
    std::cout << "  Input:  " << s << std::endl;
    std::cout << "  Output: " << out << std::endl;
    assert(out.find("नमस्ते") != std::string::npos);
    assert(out.find("भारत") != std::string::npos);

    // 3. Candidate generation speed test
    const int iterations = 100000;
    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto cands = engine.get_candidates("namaste", 5);
        (void)cands;
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    double per_query_us = (elapsed_ms * 1000.0) / iterations;
    std::cout << "  ⚡ Throughput: " << iterations << " lookups in " << elapsed_ms << " ms" << std::endl;
    std::cout << "  ⚡ Latency per transliteration: " << per_query_us << " microseconds (µs)!" << std::endl;

    // 4. Test Grapheme Cluster Removal (Backspace logical character deletion)
    std::string hindi_text = "नमस्ते";
    std::cout << "\nTesting Grapheme Cluster Backspace on: " << hindi_text << std::endl;
    while (!hindi_text.empty()) {
        bool ok = TransliterationEngine::remove_last_grapheme(hindi_text);
        assert(ok);
        std::cout << "  After ⌫: [" << hindi_text << "]" << std::endl;
    }
    std::cout << "  ✓ Grapheme cluster Backspace: PASSED" << std::endl;

    // 5. Test Word Removal (Ctrl+Backspace)
    std::string phrase = "मेरा भारत महान है";
    std::cout << "\nTesting Word Removal (Ctrl+Backspace) on: [" << phrase << "]" << std::endl;
    while (!phrase.empty()) {
        bool ok = TransliterationEngine::remove_last_word(phrase);
        assert(ok);
        std::cout << "  After Ctrl+⌫: [" << phrase << "]" << std::endl;
    }
    std::cout << "  ✓ Word removal (Ctrl+Backspace): PASSED" << std::endl;

    std::cout << "\n==============================================" << std::endl;
    std::cout << " ALL TESTS & BENCHMARKS PASSED IN C++!" << std::endl;
    std::cout << "==============================================" << std::endl;
    return 0;
}
