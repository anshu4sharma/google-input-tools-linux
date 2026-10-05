#!/usr/bin/env python3
"""
Compiles a complete Hindi transliteration and vocabulary dictionary for Google Input Tools Linux.
Aggregates:
1. Google Research Dakshina dataset (Full 276 MB release):
   - lexicons/ (hi.translit.sampled.*.tsv)
   - romanized/ (hi.romanized.rejoined.aligned.cased_nopunct.tsv & parallel corpora)
   - native_script_wikipedia/ (empirical word frequency weighting)
2. AI4Bharat Aksharantar dataset (hin_train.json, hin_valid.json, hin_test.json)
3. Shreeshrii Hindi Hunspell 228k+ vocabulary (hi_IN_large.dic)
4. Built-in curated Devanagari high-priority entries

Outputs:
- linux/assets/dict/hi_t13n.bin (PackedDict binary trie)
- linux/assets/dict/hi_complete_corpus.tsv (TSV summary)
"""

import os
import sys
import json
import zipfile
import gzip
import struct
import re
from collections import defaultdict, Counter

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(SCRIPT_DIR)
DICT_DIR = os.path.join(SCRIPT_DIR, "assets", "dict")
DAKSHINA_DIR = os.path.join(DICT_DIR, "dakshina_hi")
os.makedirs(DICT_DIR, exist_ok=True)

VOWELS = {
    0x0905: 'a', 0x0906: 'aa', 0x0907: 'i', 0x0908: 'ee', 0x0909: 'u', 0x090A: 'oo',
    0x090B: 'ri', 0x090F: 'e', 0x0910: 'ai', 0x0913: 'o', 0x0914: 'au',
    0x0902: 'n', 0x0903: 'h'
}
MATRAS = {
    0x093E: 'aa', 0x093F: 'i', 0x0940: 'ee', 0x0941: 'u', 0x0942: 'oo',
    0x0943: 'ri', 0x0947: 'e', 0x0948: 'ai', 0x094B: 'o', 0x094C: 'au'
}
CONSONANTS = {
    0x0915: 'k', 0x0916: 'kh', 0x0917: 'g', 0x0918: 'gh', 0x0919: 'ng',
    0x091A: 'ch', 0x091B: 'chh', 0x091C: 'j', 0x091D: 'jh', 0x091E: 'ny',
    0x091F: 't', 0x0920: 'th', 0x0921: 'd', 0x0922: 'dh', 0x0923: 'n',
    0x0924: 't', 0x0925: 'th', 0x0926: 'd', 0x0927: 'dh', 0x0928: 'n',
    0x092A: 'p', 0x092B: 'ph', 0x092C: 'b', 0x092D: 'bh', 0x092E: 'm',
    0x092F: 'y', 0x0930: 'r', 0x0932: 'l', 0x0933: 'l', 0x0935: 'v',
    0x0936: 'sh', 0x0937: 'sh', 0x0938: 's', 0x0939: 'h',
    0x0958: 'q', 0x0959: 'kh', 0x095A: 'g', 0x095B: 'z', 0x095C: 'd', 0x095D: 'dh', 0x095E: 'f', 0x095F: 'y'
}

def deva_to_roman(text):
    res = []
    i = 0
    n = len(text)
    while i < n:
        cp = ord(text[i])
        if cp in VOWELS:
            res.append(VOWELS[cp])
        elif cp in CONSONANTS:
            res.append(CONSONANTS[cp])
            if i + 1 < n:
                ncp = ord(text[i+1])
                if ncp == 0x094D:
                    i += 1
                elif ncp in MATRAS:
                    res.append(MATRAS[ncp])
                    i += 1
                elif ncp == 0x0902:
                    res.append('a')
                else:
                    res.append('a')
            else:
                pass
        elif cp in MATRAS:
            res.append(MATRAS[cp])
        i += 1
    return ''.join(res)

def clean_roman(word):
    if not word:
        return ""
    w = word.strip().lower()
    cleaned = ''.join(c for c in w if 'a' <= c <= 'z')
    return cleaned

def clean_devanagari(word):
    if not word:
        return ""
    # Strip common sentence punctuation attached to words like 'है।' or 'है,' or quotes
    w = word.strip(" \t\n\r।,.-:;\"'?!()[]{}")
    return w

def is_valid_devanagari(text):
    if not text:
        return False
    has_deva = False
    for ch in text:
        cp = ord(ch)
        if 0x0900 <= cp <= 0x097F:
            has_deva = True
        elif ch in (' ', '-', '।', '.'):
            continue
        elif cp < 128 and not ch.isspace():
            return False
    return has_deva

def main():
    print("=== Building Complete Hindi Language & Devanagari Dictionary ===")

    # Structure: dict[roman_key] -> dict[hindi_word] -> score
    store = defaultdict(lambda: defaultdict(int))

    # Optional: Gather real-world Hindi Wikipedia frequencies from Dakshina native text
    wiki_freqs = Counter()
    wiki_files = [
        os.path.join(DAKSHINA_DIR, "native_script_wikipedia", "hi.wiki-filt.valid.text.shuf.txt.gz"),
        os.path.join(DAKSHINA_DIR, "native_script_wikipedia", "hi.wiki-filt.train.text.shuf.txt.gz")
    ]
    for wpath in wiki_files:
        if os.path.exists(wpath):
            print(f"Sampling empirical frequencies from {os.path.basename(wpath)}...")
            try:
                with gzip.open(wpath, "rt", encoding="utf-8") as f:
                    for i, line in enumerate(f):
                        if i >= 150000: break
                        for token in line.strip().split():
                            cw = clean_devanagari(token)
                            if is_valid_devanagari(cw):
                                wiki_freqs[cw] += 1
            except Exception as e:
                print(f"Warning reading wiki text: {e}")

    print(f"-> Empirical Hindi word frequencies gathered: {len(wiki_freqs):,}")

    # 1. Ingest Google Research Dakshina Lexicons
    dakshina_files = [
        os.path.join(DAKSHINA_DIR, "lexicons", "hi.translit.sampled.train.tsv"),
        os.path.join(DAKSHINA_DIR, "lexicons", "hi.translit.sampled.dev.tsv"),
        os.path.join(DAKSHINA_DIR, "lexicons", "hi.translit.sampled.test.tsv"),
        os.path.join(DICT_DIR, "raw", "hi.translit.sampled.train.tsv"),
        os.path.join(DICT_DIR, "raw", "hi.translit.sampled.dev.tsv"),
        os.path.join(DICT_DIR, "raw", "hi.translit.sampled.test.tsv")
    ]
    seen_dakshina = set()
    dakshina_count = 0
    for path in dakshina_files:
        if not os.path.exists(path) or path in seen_dakshina:
            continue
        seen_dakshina.add(path)
        print(f"Loading Dakshina lexicon: {path}...")
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                parts = line.strip().split("\t")
                if len(parts) >= 2:
                    deva = clean_devanagari(parts[0])
                    roman = clean_roman(parts[1])
                    freq = int(parts[2]) if len(parts) >= 3 and parts[2].isdigit() else 3
                    if roman and is_valid_devanagari(deva):
                        bonus = min(wiki_freqs.get(deva, 0), 5000)
                        score = 85000 + freq * 1000 + bonus
                        store[roman][deva] = max(store[roman][deva], score)
                        dakshina_count += 1
                        if len(roman) > 2 and roman.endswith('a'):
                            short_r = roman[:-1]
                            store[short_r][deva] = max(store[short_r][deva], score - 100)
    print(f"-> Dakshina lexicon entries loaded: {dakshina_count}")

    # 2. Ingest Google Research Dakshina Romanized Parallel Corpora
    aligned_files = [
        os.path.join(DAKSHINA_DIR, "romanized", "hi.romanized.rejoined.aligned.cased_nopunct.tsv"),
        os.path.join(DAKSHINA_DIR, "romanized", "hi.romanized.rejoined.aligned.tsv")
    ]
    aligned_count = 0
    for apath in aligned_files:
        if not os.path.exists(apath):
            continue
        print(f"Loading Dakshina parallel aligned word corpus: {os.path.basename(apath)}...")
        with open(apath, "r", encoding="utf-8") as f:
            for line in f:
                parts = line.strip().split("\t")
                if len(parts) >= 2:
                    deva = clean_devanagari(parts[0])
                    roman = clean_roman(parts[1])
                    if roman and is_valid_devanagari(deva):
                        bonus = min(wiki_freqs.get(deva, 0), 5000)
                        score = 75000 + bonus
                        if deva in store[roman]:
                            store[roman][deva] += 50
                        else:
                            store[roman][deva] = score
                        aligned_count += 1
                        if len(roman) > 2 and roman.endswith('a'):
                            short_r = roman[:-1]
                            store[short_r][deva] = max(store[short_r][deva], score - 100)
        break # One aligned corpus file is sufficient since they overlap
    print(f"-> Dakshina parallel aligned word pairs loaded: {aligned_count}")

    # 3. Ingest AI4Bharat Aksharantar Dataset
    zip_paths = [
        os.path.join(DICT_DIR, "raw", "hin.zip"),
        os.path.join(ROOT_DIR, "hin.zip")
    ]
    aksharantar_count = 0
    for zpath in zip_paths:
        if os.path.exists(zpath):
            print(f"Loading AI4Bharat Aksharantar dataset: {zpath}...")
            with zipfile.ZipFile(zpath) as z:
                for member in ["hin_valid.json", "hin_test.json", "hin_train.json"]:
                    if member not in z.namelist():
                        continue
                    print(f"  Streaming {member}...")
                    with z.open(member) as f:
                        for line in f:
                            try:
                                obj = json.loads(line)
                                roman = clean_roman(obj.get("english word", ""))
                                deva = clean_devanagari(obj.get("native word", ""))
                                if roman and is_valid_devanagari(deva):
                                    bonus = min(wiki_freqs.get(deva, 0), 3000)
                                    score = 60000 + bonus
                                    if deva in store[roman]:
                                        store[roman][deva] += 50
                                    else:
                                        store[roman][deva] = score
                                    aksharantar_count += 1
                                    if len(roman) > 2 and roman.endswith('a'):
                                        short_r = roman[:-1]
                                        if deva in store[short_r]:
                                            store[short_r][deva] += 40
                                        else:
                                            store[short_r][deva] = score - 100
                            except Exception:
                                continue
            break
    print(f"-> Aksharantar entries loaded: {aksharantar_count}")

    # 4. Ingest Shreeshrii Hunspell Hindi Dictionary
    hunspell_path = os.path.join(DICT_DIR, "hi_IN_large.dic")
    hunspell_count = 0
    if os.path.exists(hunspell_path):
        print(f"Loading Hunspell Hindi dictionary: {hunspell_path}...")
        with open(hunspell_path, "r", encoding="utf-8", errors="ignore") as f:
            for line in f:
                word = line.strip()
                if not word or word.startswith("#"):
                    continue
                slash = word.find('/')
                if slash != -1:
                    word = word[:slash]
                cw = clean_devanagari(word)
                if not is_valid_devanagari(cw):
                    continue
                roman1 = deva_to_roman(cw)
                cr1 = clean_roman(roman1)
                if cr1:
                    bonus = min(wiki_freqs.get(cw, 0), 10000)
                    score = 30000 + bonus
                    store[cr1][cw] = max(store[cr1][cw], score)
                    hunspell_count += 1
                    if len(cr1) > 2 and cr1.endswith('a'):
                        short_r = cr1[:-1]
                        store[short_r][cw] = max(store[short_r][cw], score - 100)
                    alt = cr1.replace('ee', 'i').replace('oo', 'u').replace('aa', 'a')
                    if alt != cr1:
                        store[alt][cw] = max(store[alt][cw], score - 500)
    print(f"-> Hunspell vocabulary words loaded: {hunspell_count}")

    # 5. High-Priority Curated Hindi Core Words
    high_priority_words = {
        "namaste": ["नमस्ते", "नमसते", "नामस्ते"],
        "bharat": ["भारत", "भरत"],
        "kya": ["क्या"],
        "hai": ["है", "हैं"],
        "hain": ["हैं", "है"],
        "kaise": ["कैसे"],
        "aap": ["आप"],
        "tum": ["तुम"],
        "main": ["मैं", "मेन"],
        "hum": ["हम"],
        "nahi": ["नहीं", "नही"],
        "nahin": ["नहीं"],
        "accha": ["अच्छा"],
        "achha": ["अच्छा"],
        "dost": ["दोस्त"],
        "dhanyawad": ["धन्यवाद"],
        "dhanyavad": ["धन्यवाद"],
        "shukriya": ["शुक्रिया"],
        "ghar": ["घर"],
        "desh": ["देश"],
        "hindi": ["हिन्दी", "हिंदी"],
        "duniya": ["दुनिया"],
        "samay": ["समय"],
        "kaam": ["काम"],
        "shanti": ["शांति", "शान्ति"],
        "khushi": ["खुशी", "ख़ुशी"],
        "pyaar": ["प्यार"],
        "pyar": ["प्यार"],
        "zindagi": ["ज़िंदगी", "जिंदगी"],
        "kitab": ["किताब"],
        "kitaab": ["किताब"],
        "pani": ["पानी"],
        "paani": ["पानी"],
        "khana": ["खाना"],
        "swagat": ["स्वागत"],
        "alvida": ["अलविदा"],
        "kripya": ["कृपया"],
        "aaj": ["आज"],
        "kal": ["कल"],
        "parson": ["परसों"]
    }
    for r, cands in high_priority_words.items():
        base = 200000
        for i, c in enumerate(cands):
            store[r][c] = base - i * 100

    print(f"Total unique Roman keys assembled: {len(store):,}")

    max_cands_per_key = 6
    final_dict = {}
    total_candidates = 0

    for r_key, cand_map in store.items():
        if len(r_key) < 1 or len(r_key) > 40:
            continue
        sorted_cands = sorted(cand_map.items(), key=lambda x: x[1], reverse=True)
        top = sorted_cands[:max_cands_per_key]
        final_dict[r_key] = top
        total_candidates += len(top)

    print(f"Final dictionary keys: {len(final_dict):,}, total candidates: {total_candidates:,}")

    # Write TSV
    tsv_out = os.path.join(DICT_DIR, "hi_complete_corpus.tsv")
    print(f"Writing TSV corpus to {tsv_out}...")
    with open(tsv_out, "w", encoding="utf-8") as f:
        f.write("# Roman\tDevanagari\tFrequency\n")
        for r_key in sorted(final_dict.keys()):
            for cand, score in final_dict[r_key]:
                f.write(f"{r_key}\t{cand}\t{score}\n")

    # Write Binary PackedDict (GIT1 format)
    bin_out = os.path.join(DICT_DIR, "hi_t13n.bin")
    print(f"Writing binary PackedDict to {bin_out}...")
    with open(bin_out, "wb") as f:
        f.write(b"GIT1")
        f.write(struct.pack("<I", 1)) # version = 1
        f.write(struct.pack("<I", len(final_dict)))

        for r_key, cands in final_dict.items():
            k_bytes = r_key.encode("ascii")
            f.write(struct.pack("<H", len(k_bytes)))
            f.write(k_bytes)
            f.write(struct.pack("<B", len(cands)))
            for cand, score in cands:
                c_bytes = cand.encode("utf-8")
                f.write(struct.pack("<H", len(c_bytes)))
                f.write(c_bytes)
                f.write(struct.pack("<I", min(score, 0xFFFFFFFF)))

    file_size_mb = os.path.getsize(bin_out) / (1024 * 1024)
    print(f"✓ Successfully generated {bin_out} ({file_size_mb:.2f} MB)")

if __name__ == "__main__":
    main()
