#include "../settings.h"

#include <cstring>
#include <iostream>

namespace {

int gFailures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::cerr << __FILE__ << ':' << __LINE__                           \
                      << ": check failed: " #condition << '\n';                \
            ++gFailures;                                                        \
        }                                                                       \
    } while (false)

void CheckCrc32Vector()
{
    // The standard check value for CRC-32/ISO-HDLC.
    CHECK(Mss::Settings::Crc32Bytes("123456789", 9) == 0xCBF43926u);
    CHECK(Mss::Settings::Crc32Bytes("", 0) == 0u);
}

void CheckCrcCoversOnlyTheMacroShape()
{
    Mss::Settings::Init(nullptr);  // defaults, no storage
    const uint32_t base = Mss::Settings::Crc32();
    CHECK(base == Mss::Settings::Crc32());

    Mss::Settings::SetPauseOnRelease(false);
    Mss::Settings::SetMacroEnabled(false);
    CHECK(Mss::Settings::Crc32() == base);  // display and gating do not count

    Mss::Settings::PreviewPlusDelayMs(150);
    const uint32_t delayed = Mss::Settings::Crc32();
    CHECK(delayed != base);

    Mss::Settings::SetStickEnabled(false);
    CHECK(Mss::Settings::Crc32() != delayed);

    Mss::Settings::SetStickEnabled(true);
    Mss::Settings::PreviewPlusDelayMs(100);
    CHECK(Mss::Settings::Crc32() == base);
}

} // namespace

int main()
{
    CheckCrc32Vector();
    CheckCrcCoversOnlyTheMacroShape();

    if (gFailures != 0) {
        std::cerr << gFailures << " settings test(s) failed\n";
        return 1;
    }
    std::cout << "settings tests passed\n";
    return 0;
}
