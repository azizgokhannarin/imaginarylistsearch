#include <cstdint>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

// -------------------- PRP16: 4-round Feistel over 8+8 bits --------------------
// Bijective => MSB test yields exactly 32768 members per list.
static inline uint8_t F(uint8_t r, uint32_t k, int round)
{
    // Cheap nonlinear-ish mixing, not crypto; just diffusion.
    uint32_t x = (uint32_t)r;
    x ^= (k >> (round * 8)) & 0xFFu;
    x *= 0x45D9F3Bu;              // mix
    x ^= x >> 16;
    x *= 0x45D9F3Bu;
    x ^= x >> 16;
    return (uint8_t)x;
}

static inline uint16_t prp16(uint16_t x, uint32_t key)
{
    uint8_t L = (uint8_t)(x >> 8);
    uint8_t R = (uint8_t)(x & 0xFFu);

    // 4-round Feistel
    for (int r = 0; r < 4; ++r) {
        uint8_t t = (uint8_t)(L ^ F(R, key, r));
        L = R;
        R = t;
    }
    return (uint16_t)((uint16_t)L << 8) | (uint16_t)R;
}

// Membership: MSB(prp16(x)) == 0  => exactly half of the universe
static inline bool hasValue(uint32_t listID, uint16_t x)
{
    uint16_t y = prp16(x, listID);
    return (y & 0x8000u) == 0;
}

// -------------------- Scoring --------------------
static inline int score_list(uint32_t listID, const uint16_t *data, size_t n)
{
    int s = 0;
    for (size_t i = 0; i < n; ++i) s += hasValue(listID, data[i]) ? 1 : 0;
    // Eğer yarısından fazlası yanlış taraftaysa, o anahtarın aslında 'tersi' daha başarılıdır
    return std::max(s, (int)n - s);
}

// -------------------- Search: random restarts + greedy bit-flip hillclimb --------------------
struct SearchConfig {
    int restarts = 200;        // number of random initial keys
    int hillIters = 6;         // how many hillclimb passes
    bool tryAllBits = true;    // flip all 32 bits each pass
};

struct SearchResult {
    uint32_t bestID = 0;
    int bestScore = -1;
};

static SearchResult searchBestListID(const std::vector<uint16_t> &block, const SearchConfig &cfg,
                                     uint64_t rngSeed = 0xC0FFEE123456789ull)
{
    std::mt19937_64 rng(rngSeed);
    std::uniform_int_distribution<uint32_t> dist32(0u, 0xFFFFFFFFu);

    SearchResult res;

    // Helper: greedy hillclimb starting from a key
    auto hillclimb = [&](uint32_t startID) -> std::pair<uint32_t, int> {
        uint32_t cur = startID;
        int curScore = score_list(cur, block.data(), block.size());

        for (int pass = 0; pass < cfg.hillIters; ++pass)
        {
            bool improved = false;

            // Try flipping bits (either all 32, or a subset)
            for (int b = 0; b < 32; ++b) {
                uint32_t cand = cur ^ (1u << b);
                int sc = score_list(cand, block.data(), block.size());
                if (sc > curScore) {
                    cur = cand;
                    curScore = sc;
                    improved = true;
                }
            }

            if (!improved) break;
        }
        return {cur, curScore};
    };

    // Random restarts
    for (int r = 0; r < cfg.restarts; ++r) {
        uint32_t start = dist32(rng);
        auto [id, sc] = hillclimb(start);
        if (sc > res.bestScore) {
            res.bestScore = sc;
            res.bestID = id;
        }
    }

    return res;
}

// -------------------- File helper: read as uint16_t LE --------------------
static bool read_u16_le_file(const std::string &path, std::vector<uint16_t> &out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t n = bytes.size() / 2;
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out[i] = (uint16_t)bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8);
    }
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file> [blockLen=64] [blocksToTest=50]\n";
        return 1;
    }

    std::string path = argv[1];
    size_t blockLen = (argc >= 3) ? (size_t)std::stoul(argv[2]) : 64;
    size_t blocksToTest = (argc >= 4) ? (size_t)std::stoul(argv[3]) : 50;

    std::vector<uint16_t> data;
    if (!read_u16_le_file(path, data)) {
        std::cerr << "Cannot read: " << path << "\n";
        return 2;
    }
    if (data.size() < blockLen) {
        std::cerr << "File too small for blockLen.\n";
        return 3;
    }

    SearchConfig cfg;
    cfg.restarts = 250;   // tune
    cfg.hillIters = 8;

    std::cout << "u16 count: " << data.size() << "\n";
    std::cout << "blockLen: " << blockLen << ", blocksToTest: " << blocksToTest << "\n";
    std::cout << "search: restarts=" << cfg.restarts << " hillIters=" << cfg.hillIters << "\n\n";

    // Test multiple consecutive blocks
    size_t maxBlocks = std::min(blocksToTest, (data.size() / blockLen));
    double avgScore = 0.0;
    int bestEver = -1;
    uint32_t bestEverID = 0;

    // 1 No manipulation
    for (size_t bi = 0; bi < maxBlocks; ++bi) {
        std::vector<uint16_t> block(data.begin() + bi * blockLen, data.begin() + (bi + 1)*blockLen);

        auto r = searchBestListID(block, cfg, 0xC0FFEE123456789ull + bi);
        avgScore += (double)r.bestScore;

        if (r.bestScore > bestEver) {
            bestEver = r.bestScore;
            bestEverID = r.bestID;
        }

        std::cout << "Block " << bi
                  << " bestScore=" << r.bestScore << "/" << blockLen
                  << "  listID=0x" << std::hex << std::setw(8) << std::setfill('0') << r.bestID
                  << std::dec << "\n";
    }

    avgScore /= (double)maxBlocks;
    std::cout << "1 No Manipulation:" << std::endl;
    std::cout << "\nAverage bestScore: " << std::fixed << std::setprecision(2) << avgScore
              << "/" << blockLen << " (" << (100.0 * avgScore / (double)blockLen) << "%)\n";
    std::cout << "Best ever: " << bestEver << "/" << blockLen
              << "  listID=0x" << std::hex << std::setw(8) << std::setfill('0') << bestEverID
              << std::dec << "\n";

    return 0;
}
