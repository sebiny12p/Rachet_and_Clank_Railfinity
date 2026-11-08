#pragma once
#include <cstdint>
#include <ctime>
#include <cstdio>
#include <string>

namespace Railfinity {

class PCG32 {
public:
    explicit PCG32(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t seq = 0xda3e39cb94b95bdbULL) {
        Seed(seed, seq);
    }

    void Seed(uint64_t initstate, uint64_t initseq = 54u) {
        state_ = 0U;
        inc_ = (initseq << 1u) | 1u;
        NextUInt();
        state_ += initstate;
        NextUInt();
    }

    uint32_t NextUInt() {
        uint64_t oldstate = state_;
        state_ = oldstate * 6364136223846793005ULL + inc_;
        uint32_t xorshifted = static_cast<uint32_t>(((oldstate >> 18u) ^ oldstate) >> 27u);
        uint32_t rot = static_cast<uint32_t>(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    float NextFloat() {
        return static_cast<float>(NextUInt() >> 8) * (1.0f / 16777216.0f);
    }

    float RangeFloat(float minVal, float maxVal) {
        return minVal + NextFloat() * (maxVal - minVal);
    }

    int RangeInt(int minVal, int maxVal) {
        if (maxVal <= minVal) return minVal;
        uint32_t span = static_cast<uint32_t>(maxVal - minVal + 1);
        return minVal + static_cast<int>(NextUInt() % span);
    }

    bool Chance(float probability) {
        return NextFloat() < probability;
    }

    static uint64_t GetDailyChallengeSeed() {
        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        if (!lt) return 20261108ULL;
        uint64_t dateCode = static_cast<uint64_t>(lt->tm_year + 1900) * 10000ULL +
                            static_cast<uint64_t>(lt->tm_mon + 1) * 100ULL +
                            static_cast<uint64_t>(lt->tm_mday);
        uint64_t z = (dateCode + 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    static std::string GetDailyDateString() {
        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        if (!lt) return "2026-11-08";
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                      lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
        return std::string(buf);
    }

private:
    uint64_t state_ = 0;
    uint64_t inc_ = 0;
};

} // namespace Railfinity
