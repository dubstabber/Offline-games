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
    assert(back.sokobanLevelEasy == def.sokobanLevelEasy);
    assert(back.sokobanLevelMedium == def.sokobanLevelMedium);
    assert(back.sokobanLevelHard == def.sokobanLevelHard);
    assert(back.nibblesLevelEasy == def.nibblesLevelEasy);
    assert(back.nibblesLevelMedium == def.nibblesLevelMedium);
    assert(back.nibblesLevelHard == def.nibblesLevelHard);
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
    s.sokobanLevelEasy = 9;
    s.sokobanLevelMedium = 17;
    s.sokobanLevelHard = 5;
    s.nibblesLevelEasy = 4;
    s.nibblesLevelMedium = 15;
    s.nibblesLevelHard = 26;
    const Settings back = parse(serialize(s));
    assert(back.darkMode);
    assert(nearly(back.volume, 0.35F));
    assert(!back.music);
    assert(!back.vibration);
    assert(back.maxFps == 60);
    assert(back.tapmatchLevelEasy == 7);
    assert(back.tapmatchLevelMedium == 42);
    assert(back.tapmatchLevelHard == 3);
    assert(back.sokobanLevelEasy == 9);
    assert(back.sokobanLevelMedium == 17);
    assert(back.sokobanLevelHard == 5);
    assert(back.nibblesLevelEasy == 4);
    assert(back.nibblesLevelMedium == 15);
    assert(back.nibblesLevelHard == 26);
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

// Per-difficulty Snake best scores persist, floor at 0, and survive round-trips.
void testSnakeBest() {
    assert(parse("snakeBestEasy=120").snakeBestEasy == 120);
    assert(parse("snakeBestMedium=4500").snakeBestMedium == 4500);
    assert(parse("snakeBestHard=77").snakeBestHard == 77);
    assert(parse("snakeBestEasy=-5").snakeBestEasy == 0); // floor at 0
    const Settings def;
    assert(parse("snakeBestHard=nope").snakeBestHard == def.snakeBestHard);
    Settings s;
    s.snakeBestEasy = 11;
    s.snakeBestMedium = 222;
    s.snakeBestHard = 3333;
    const Settings back = parse(serialize(s));
    assert(back.snakeBestEasy == 11);
    assert(back.snakeBestMedium == 222);
    assert(back.snakeBestHard == 3333);
}

// Per-difficulty Hole best scores persist, floor at 0, and survive round-trips.
void testHoleBest() {
    assert(parse("holeBestEasy=120").holeBestEasy == 120);
    assert(parse("holeBestMedium=4500").holeBestMedium == 4500);
    assert(parse("holeBestHard=77").holeBestHard == 77);
    assert(parse("holeBestEasy=-5").holeBestEasy == 0);
    const Settings def;
    assert(parse("holeBestHard=nope").holeBestHard == def.holeBestHard);
    Settings s;
    s.holeBestEasy = 11;
    s.holeBestMedium = 222;
    s.holeBestHard = 3333;
    const Settings back = parse(serialize(s));
    assert(back.holeBestEasy == 11);
    assert(back.holeBestMedium == 222);
    assert(back.holeBestHard == 3333);
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

// Per-difficulty Sokoban progress persists and is floored at level 1.
void testSokobanLevels() {
    assert(parse("sokobanLevelEasy=7").sokobanLevelEasy == 7);
    assert(parse("sokobanLevelMedium=12").sokobanLevelMedium == 12);
    assert(parse("sokobanLevelHard=3").sokobanLevelHard == 3);
    assert(parse("sokobanLevelEasy=0").sokobanLevelEasy == 1);
    assert(parse("sokobanLevelHard=-9").sokobanLevelHard == 1);
    const Settings def;
    assert(parse("sokobanLevelMedium=oops").sokobanLevelMedium == def.sokobanLevelMedium);
}

// Per-difficulty Nibbles progress persists and is floored at level 1.
void testNibblesLevels() {
    assert(parse("nibblesLevelEasy=7").nibblesLevelEasy == 7);
    assert(parse("nibblesLevelMedium=12").nibblesLevelMedium == 12);
    assert(parse("nibblesLevelHard=26").nibblesLevelHard == 26);
    assert(parse("nibblesLevelEasy=0").nibblesLevelEasy == 1);
    assert(parse("nibblesLevelHard=-9").nibblesLevelHard == 1);
    const Settings def;
    assert(parse("nibblesLevelMedium=oops").nibblesLevelMedium == def.nibblesLevelMedium);
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
    testSokobanLevels();
    testNibblesLevels();
    testSnakeBest();
    testHoleBest();
    std::puts("All Settings tests passed.");
    return 0;
}
