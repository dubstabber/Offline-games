#include "core/Settings.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

using og::parse;
using og::serialize;
using og::Settings;

bool nearly(float a, float b) {
    return std::fabs(a - b) < 0.01F;
}

// Default settings survive a serialize → parse round-trip unchanged.
void testRoundTripDefaults() {
    const Settings def;
    const Settings back = parse(serialize(def));
    assert(back.darkMode == def.darkMode);
    assert(nearly(back.volume, def.volume));
    assert(back.music == def.music);
    assert(back.vibration == def.vibration);
    assert(back.maxFps == def.maxFps);
    assert(back.tapmatchLevelEasy == def.tapmatchLevelEasy);
    assert(back.tapmatchLevelMedium == def.tapmatchLevelMedium);
    assert(back.tapmatchLevelHard == def.tapmatchLevelHard);
}

// Custom values survive the round-trip too (volume to 2 decimals).
void testRoundTripCustom() {
    Settings s;
    s.darkMode = true;
    s.volume = 0.35F;
    s.music = false;
    s.vibration = false;
    s.maxFps = 60;
    s.tapmatchLevelEasy = 7;
    s.tapmatchLevelMedium = 42;
    s.tapmatchLevelHard = 3;
    const Settings back = parse(serialize(s));
    assert(back.darkMode);
    assert(nearly(back.volume, 0.35F));
    assert(!back.music);
    assert(!back.vibration);
    assert(back.maxFps == 60);
    assert(back.tapmatchLevelEasy == 7);
    assert(back.tapmatchLevelMedium == 42);
    assert(back.tapmatchLevelHard == 3);
}

// Empty or whitespace-only input yields the defaults.
void testEmptyIsDefaults() {
    const Settings def;
    const Settings a = parse("");
    const Settings b = parse("\n\n   \n");
    assert(a.maxFps == def.maxFps && nearly(a.volume, def.volume));
    assert(b.darkMode == def.darkMode && b.music == def.music);
}

// Unknown keys, blank lines, and malformed lines are ignored; valid keys apply.
void testToleratesGarbage() {
    const Settings s = parse("darkMode=1\nbogusKey=42\nnot a kv line\n=missingkey\nmaxFps=90\n");
    assert(s.darkMode);
    assert(s.maxFps == 90);
}

// CRLF line endings are tolerated.
void testCrlf() {
    const Settings s = parse("darkMode=1\r\nmaxFps=30\r\n");
    assert(s.darkMode);
    assert(s.maxFps == 30);
}

// Out-of-range values are clamped/snapped so parse always yields a valid config.
void testClampingAndSnapping() {
    assert(nearly(parse("volume=5.0").volume, 1.0F));
    assert(nearly(parse("volume=-2.0").volume, 0.0F));
    assert(parse("maxFps=10").maxFps == 30);       // below the lowest stop
    assert(parse("maxFps=70").maxFps == 60);       // nearest is 60
    assert(parse("maxFps=80").maxFps == 90);       // nearest is 90
    assert(parse("maxFps=1000").maxFps == 120);    // above the highest stop
    const int snapped = parse("maxFps=45").maxFps; // tie between 30 and 60
    assert(snapped == 30 || snapped == 60);
}

// A malformed value keeps that field's default instead of corrupting it.
void testMalformedValueKeepsDefault() {
    const Settings def;
    const Settings s = parse("maxFps=abc\nvolume=xyz\n");
    assert(s.maxFps == def.maxFps);
    assert(nearly(s.volume, def.volume));
}

// Per-difficulty Tap Match progress persists and is floored at level 1.
void testTapmatchLevels() {
    assert(parse("tapmatchLevelEasy=7").tapmatchLevelEasy == 7);
    assert(parse("tapmatchLevelMedium=12").tapmatchLevelMedium == 12);
    assert(parse("tapmatchLevelHard=3").tapmatchLevelHard == 3);
    assert(parse("tapmatchLevelEasy=0").tapmatchLevelEasy == 1);  // floor at 1
    assert(parse("tapmatchLevelHard=-9").tapmatchLevelHard == 1); // floor at 1
    const Settings def;
    assert(parse("tapmatchLevelMedium=oops").tapmatchLevelMedium == def.tapmatchLevelMedium);
}

} // namespace

int main() {
    testRoundTripDefaults();
    testRoundTripCustom();
    testEmptyIsDefaults();
    testToleratesGarbage();
    testCrlf();
    testClampingAndSnapping();
    testMalformedValueKeepsDefault();
    testTapmatchLevels();
    std::puts("All Settings tests passed.");
    return 0;
}
