# cgzip

## Requirements

| Requirement | Status |
|-------------|--------|
| README.md | ✅ |
| LZSS to generate blocks of type 1 and 2 | ✅ |
| LZSS with a look-back of 32768 and look-ahead of 258 | ✅ |
| Compresses the entire data/ folder in less than 240 seconds | ✅ |
| Huffman coding to generate dynamic prefix codes for block type 2, based on actual symbol frequencies | ✅ |
| Compression ratio consistently higher than `gzip -9` | ❌ |
| Compression ratio consistently higher than `gzip -5` | 🟡 |
| Compression ratio consistently higher than `gzip -1` | ✅ |
| Advanced optimizations for LZSS, including a rolling window to avoid running out of memory | ✅ |
| Optimized block 2 header | ✅ |
| Avoids emitting backreferences when literals are cheaper | ✅ |
| Adaptive block sizing | ✅ |
| Adaptive block type selection | ✅ |

## Speed

Based on the following hardware:

```
          ▗▄▄▄       ▗▄▄▄▄    ▄▄▄▖             callumcurtis@mist
          ▜███▙       ▜███▙  ▟███▛             -----------------
           ▜███▙       ▜███▙▟███▛              OS: NixOS 25.05 (Warbler) x86_64
            ▜███▙       ▜██████▛               Host: Surface Book 2
     ▟█████████████████▙ ▜████▛     ▟▙         Kernel: Linux 6.12.45
    ▟███████████████████▙ ▜███▙    ▟██▙        Uptime: 31 mins
           ▄▄▄▄▖           ▜███▙  ▟███▛        Packages: 2252 (nix-system)
          ▟███▛             ▜██▛ ▟███▛         Shell: bash 5.2.37
         ▟███▛               ▜▛ ▟███▛          Display (LGD0554): 3240x2160 @ 60 Hz (as 1620x1080) in 15" [Built-in]
▟███████████▛                  ▟██████████▙    WM: Hyprland 0.51.0 (Wayland)
▜██████████▛                  ▟███████████▛    Theme: adw-gtk3 [GTK2/3/4]
      ▟███▛ ▟▙               ▟███▛             Font: DejaVu Sans (10pt) [GTK2/3/4]
     ▟███▛ ▟██▙             ▟███▛              Cursor: Bibata-Original-Ice (24px)
    ▟███▛  ▜███▙           ▝▀▀▀▀               Terminal: zellij 0.43.1
    ▜██▛    ▜███▙ ▜██████████████████▛         CPU: Intel(R) Core(TM) i7-8650U (8) @ 4.20 GHz
     ▜▛     ▟████▙ ▜████████████████▛          GPU 1: NVIDIA GeForce GTX 1060 Mobile [Discrete]
           ▟██████▙       ▜███▙                GPU 2: Intel UHD Graphics 620 @ 1.15 GHz [Integrated]
          ▟███▛▜███▙       ▜███▙               Memory: 4.93 GiB / 15.54 GiB (32%)
         ▟███▛  ▜███▙       ▜███▙              Swap: Disabled
         ▝▀▀▀    ▀▀▀▀▘       ▀▀▀▘              Disk (/): 155.64 GiB / 443.57 GiB (35%) - ext4
                                               Local IP (wlp1s0): 192.168.1.199/24
                                               Battery (M1005046): 100% [AC Connected]
                                               Battery (M1010958): 100% [AC Connected]
                                               Locale: en_CA.UTF-8
```

The following performance results were achieved by compressing a `data.tar` file created from the `data/` folder.

```
❯ hyperfine './install/bin/cgzip < data.tar > data.tar.gz'
Benchmark 1: ./install/bin/cgzip < data.tar > data.tar.gz
  Time (mean ± σ):     35.629 s ±  1.466 s    [User: 35.280 s, System: 0.044 s]
  Range (min … max):   33.711 s … 37.568 s    10 runs
```

## Compression Ratio

The following compression ratio results were achieved across individual files inside of the `data/` folder:
```
❯ ./scripts/validate
./data/canterbury_corpus/asyoulik.txt    | PASSED | compression ratio:   249.40% | compression ratio vs. standard gzip (level 5) (relative %): -1.11%
./data/canterbury_corpus/kennedy.xls     | PASSED | compression ratio:   506.68% | compression ratio vs. standard gzip (level 5) (relative %):  2.82%
./data/canterbury_corpus/alice29.txt     | PASSED | compression ratio:   274.61% | compression ratio vs. standard gzip (level 5) (relative %):  0.42%
./data/canterbury_corpus/lcet10.txt      | PASSED | compression ratio:   289.10% | compression ratio vs. standard gzip (level 5) (relative %): -0.30%
./data/canterbury_corpus/grammar.lsp     | PASSED | compression ratio:   296.73% | compression ratio vs. standard gzip (level 5) (relative %): -0.64%
./data/canterbury_corpus/ptt5            | PASSED | compression ratio:   916.85% | compression ratio vs. standard gzip (level 5) (relative %):  2.95%
./data/canterbury_corpus/fields.c        | PASSED | compression ratio:   350.08% | compression ratio vs. standard gzip (level 5) (relative %): -0.57%
./data/canterbury_corpus/xargs.1         | PASSED | compression ratio:   237.47% | compression ratio vs. standard gzip (level 5) (relative %): -1.35%
./data/canterbury_corpus/cp.html         | PASSED | compression ratio:   301.51% | compression ratio vs. standard gzip (level 5) (relative %): -1.38%
./data/canterbury_corpus/sum             | PASSED | compression ratio:   288.15% | compression ratio vs. standard gzip (level 5) (relative %): -2.10%
./data/canterbury_corpus/plrabn12.txt    | PASSED | compression ratio:   241.45% | compression ratio vs. standard gzip (level 5) (relative %): -0.24%
./data/calgary_corpus/paper2             | PASSED | compression ratio:   268.64% | compression ratio vs. standard gzip (level 5) (relative %): -1.51%
./data/calgary_corpus/paper1             | PASSED | compression ratio:   280.93% | compression ratio vs. standard gzip (level 5) (relative %): -1.17%
./data/calgary_corpus/book1              | PASSED | compression ratio:   238.95% | compression ratio vs. standard gzip (level 5) (relative %): -1.08%
./data/calgary_corpus/obj2               | PASSED | compression ratio:   291.80% | compression ratio vs. standard gzip (level 5) (relative %): -2.04%
./data/calgary_corpus/trans              | PASSED | compression ratio:   479.77% | compression ratio vs. standard gzip (level 5) (relative %): -0.84%
./data/calgary_corpus/paper5             | PASSED | compression ratio:   233.84% | compression ratio vs. standard gzip (level 5) (relative %): -2.11%
./data/calgary_corpus/news               | PASSED | compression ratio:   252.86% | compression ratio vs. standard gzip (level 5) (relative %): -2.51%
./data/calgary_corpus/progp              | PASSED | compression ratio:   430.09% | compression ratio vs. standard gzip (level 5) (relative %): -0.91%
./data/calgary_corpus/book2              | PASSED | compression ratio:   287.03% | compression ratio vs. standard gzip (level 5) (relative %): -1.62%
./data/calgary_corpus/geo                | PASSED | compression ratio:   148.10% | compression ratio vs. standard gzip (level 5) (relative %): -0.62%
./data/calgary_corpus/progl              | PASSED | compression ratio:   428.04% | compression ratio vs. standard gzip (level 5) (relative %): -1.24%
./data/calgary_corpus/progc              | PASSED | compression ratio:   290.64% | compression ratio vs. standard gzip (level 5) (relative %): -2.04%
./data/calgary_corpus/obj1               | PASSED | compression ratio:   204.90% | compression ratio vs. standard gzip (level 5) (relative %): -1.64%
./data/calgary_corpus/paper3             | PASSED | compression ratio:   249.31% | compression ratio vs. standard gzip (level 5) (relative %): -2.54%
./data/calgary_corpus/paper6             | PASSED | compression ratio:   278.73% | compression ratio vs. standard gzip (level 5) (relative %): -2.63%
./data/calgary_corpus/pic                | PASSED | compression ratio:   916.85% | compression ratio vs. standard gzip (level 5) (relative %):  2.95%
./data/calgary_corpus/bib                | PASSED | compression ratio:   305.64% | compression ratio vs. standard gzip (level 5) (relative %): -1.43%
./data/calgary_corpus/paper4             | PASSED | compression ratio:   233.13% | compression ratio vs. standard gzip (level 5) (relative %): -2.46%
./data/millbay_corpus/regional_distri    | PASSED | compression ratio:   124.55% | compression ratio vs. standard gzip (level 5) (relative %):  0.04%
./data/millbay_corpus/GCA_009858895.3    | PASSED | compression ratio:   310.97% | compression ratio vs. standard gzip (level 5) (relative %):  0.21%
./data/millbay_corpus/Historical_Date    | PASSED | compression ratio:   472.36% | compression ratio vs. standard gzip (level 5) (relative %): -0.40%
./data/millbay_corpus/compressed-file    | PASSED | compression ratio:   195.80% | compression ratio vs. standard gzip (level 5) (relative %):  2.72%
./data/millbay_corpus/pg1513.epub        | PASSED | compression ratio:   101.40% | compression ratio vs. standard gzip (level 5) (relative %): -0.05%
./data/millbay_corpus/jquery-3.6.4.js    | PASSED | compression ratio:   331.28% | compression ratio vs. standard gzip (level 5) (relative %): -0.69%
./data/millbay_corpus/README.txt         | PASSED | compression ratio:   181.74% | compression ratio vs. standard gzip (level 5) (relative %):  1.62%
./data/millbay_corpus/TheWarofTheWorl    | PASSED | compression ratio:   257.74% | compression ratio vs. standard gzip (level 5) (relative %): -0.21%
./data/millbay_corpus/pear.jpg           | PASSED | compression ratio:   100.61% | compression ratio vs. standard gzip (level 5) (relative %):  0.02%
./data/millbay_corpus/reboot.c           | PASSED | compression ratio:   360.02% | compression ratio vs. standard gzip (level 5) (relative %): -1.16%
./data/other/two_cities.txt              | PASSED | compression ratio:   216.06% | compression ratio vs. standard gzip (level 5) (relative %):  6.73%
./data/other/abc.txt                     | PASSED | compression ratio:    11.54% | compression ratio vs. standard gzip (level 5) (relative %): 19.21%
./data/other/english_words.txt           | PASSED | compression ratio:   309.21% | compression ratio vs. standard gzip (level 5) (relative %): -1.91%
./data/other/a65536.txt                  | PASSED | compression ratio: 57487.72% | compression ratio vs. standard gzip (level 5) (relative %): -5.26%
./data/other/banana.txt                  | PASSED | compression ratio:    20.69% | compression ratio vs. standard gzip (level 5) (relative %): 20.71%
./data/other/a32768.txt                  | PASSED | compression ratio: 49648.48% | compression ratio vs. standard gzip (level 5) (relative %): 15.15%
--------------------------------------------------------
45/45 passed, 0/45 failed.
Custom compressor beat standard compressor (level 5): 13/45 times.
Compression Difference Stats (%):
  Minimum: -5.26%
  Maximum: 20.71%
  Average: 0.66%
```

Crucially, on the `data.tar` file created from the `data/` folder, the custom compressor beats the standard compressor (level 5):

```
❯ make test
...
./install/bin/cgzip < data.tar > data.tar.gz
gzip -cd data.tar.gz > data.d.tar
diff data.d.tar data.tar -s
Files data.d.tar and data.tar are identical
original file size: 20940800
compressed file size: 5483205
gzip -9 file size: 0
gzip -6 file size: 5390227
gzip -5 file size: 5486475
gzip -1 file size: 6294624
```

## Features

### Huffman Coding

Package merge is used to generate Huffman code lengths based on symbol frequencies. See the [implementation](include/package_merge.hpp) and [reference](https://people.eng.unimelb.edu.au/ammoffat/abstracts/compsurv19moffat.pdf) for more details.

Huffman code lengths are translated into prefix codes according to [RFC 1951](https://www.ietf.org/rfc/rfc1951.txt). See the [implementation](include/prefix_codes.hpp) for more details.

### LZSS

Ring buffers are used to represent look-ahead and look-back buffers, ensuring
that memory overhead is upper-bounded by the maximum look-ahead and look-back
sizes. The position of the most recent occurrence of each length-three pattern 
is represented in a hash map. An additional ring buffer is used to represent 
the "chain" of additional occurrences of the same pattern in the look-back 
buffer. For example, the hash map could indicate that the pattern "ABC"
occurred at position 100 in the look-back buffer. At the same position in the
chain buffer, there could be an entry with the value 90, indicating that
the pattern "ABC" previously occurred at position 90. In this fashion,
the full set of positions for "ABC" are cached in the hash map and chain 
buffer. See the [implementation](include/lzss.hpp) for more details.

### Optimized Block Type 2 Header

The block type 2 header is optimized to reduce the number of bits required
to represent the header in the presence of trailing zero-length 
meta-prefix-codes. See the [implementation](include/block_type_2.hpp) for more details.

### Adaptive Block Sizing

The block size is dynamically determined based on a CUSUM algorithm for
change-point detection in a categorical data stream. This [article](https://sarem-seitz.com/posts/probabilistic-cusum-for-change-point-detection.html)
gives a good overview of the algorithm for the numerical data stream case.
To adapt the algorithm for the categorical data stream case, the probability
of each symbol is tracked, rather than the mean of the data stream. Using
the log-likelihood ratio, the algorithm determines whether the probability
of the current symbol is more likely given the current distribution, rather 
than the distribution from the warmup period. Once the log-likelihood ratio
has accumulated enough, the distribution of the data stream is deemed to have
shifted, and a change-point is reported. A new block is then created. See the 
[implementation](include/change_point_detection.hpp) for more details.

### Adaptive Block Type Selection

The optimal block type is selected by simulating the contents of each block 
type and comparing the number of bits. Due to the warmup period required
for change-point detection, the minimum size of a block is `2^13` bytes.
In practice, this is larger than desirable for a block of type 1, as
beyond this size, the overhead of block type 2 is relatively small,
making block type 2 preferable in essentially all cases. Since block type 1
will never be used, it is disabled to improve speed. It can be re-enabled
by increasing the breakpoint value for block type 1 from `0` to a value larger than `2^13`
in [main.cpp](app/main.cpp), allowing block type 1 to be considered.
