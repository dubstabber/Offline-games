#include "games/hexanaut/HexGrid.hpp"
#include "games/hexanaut/HexTypes.hpp"
#include "games/hexanaut/HexWorld.hpp"

#include <cassert>
#include <cstdio>

namespace {

using og::hexanaut::axialToWorld;
using og::hexanaut::dirAngle;
using og::hexanaut::HexCoord;
using og::hexanaut::HexDir;
using og::hexanaut::hexDistance;
using og::hexanaut::HexGrid;
using og::hexanaut::HexWorld;
using og::hexanaut::neighbor;
using og::hexanaut::opposite;
using og::hexanaut::PlayerId;
using og::hexanaut::quantizeToHexDir;
using og::hexanaut::Vec2;
using og::hexanaut::worldToAxial;

// Kill every bot so a free-movement test can drive player 0 in isolation.
void soloHuman(HexWorld& world) {
    const int n = static_cast<int>(world.players().size());
    for (int id = 1; id < n; ++id) {
        world.setAliveForTest(static_cast<PlayerId>(id), false);
    }
}

// The HexDir whose neighbor of `a` is `b` (a and b must be adjacent).
HexDir dirBetween(HexCoord a, HexCoord b) {
    for (int d = 0; d < 6; ++d) {
        if (neighbor(a, static_cast<HexDir>(d)) == b) {
            return static_cast<HexDir>(d);
        }
    }
    return HexDir::None;
}

void walk(HexWorld& world, HexDir dir) {
    world.setDesiredDirForTest(0, dir);
    world.advanceCellForTest();
}

void testHexMath() {
    const HexCoord o{5, 5};
    assert(neighbor(o, HexDir::N) == HexCoord(5, 4));
    assert(neighbor(o, HexDir::NE) == HexCoord(6, 4));
    assert(neighbor(o, HexDir::SE) == HexCoord(6, 5));
    assert(neighbor(o, HexDir::S) == HexCoord(5, 6));
    assert(neighbor(o, HexDir::SW) == HexCoord(4, 6));
    assert(neighbor(o, HexDir::NW) == HexCoord(4, 5));

    assert(opposite(HexDir::N) == HexDir::S);
    assert(opposite(HexDir::NE) == HexDir::SW);
    assert(opposite(HexDir::SE) == HexDir::NW);
    assert(opposite(opposite(HexDir::S)) == HexDir::S);

    assert(hexDistance(o, o) == 0);
    for (int d = 0; d < 6; ++d) {
        assert(hexDistance(o, neighbor(o, static_cast<HexDir>(d))) == 1);
    }
    assert(hexDistance({0, 0}, {3, 0}) == 3);
    assert(hexDistance({0, 0}, {0, 4}) == 4);
    assert(hexDistance({2, 2}, {5, 7}) == hexDistance({5, 7}, {2, 2})); // symmetric

    // Quantizing the exact world-space angle of each direction returns it back.
    for (int d = 0; d < 6; ++d) {
        const auto dir = static_cast<HexDir>(d);
        const Vec2 delta = axialToWorld(neighbor(o, dir), 1.0F) - axialToWorld(o, 1.0F);
        assert(quantizeToHexDir(og::hexanaut::angleOf(delta)) == dir);
    }
}

// worldToAxial is the inverse of axialToWorld: every cell center round-trips back
// to its own coordinate (this is what maps a free avatar's position to its hex).
void testWorldToAxialRoundTrip() {
    constexpr float kSize = 26.0F;
    for (const HexCoord c : {HexCoord{0, 0}, HexCoord{5, 5}, HexCoord{20, 13}, HexCoord{40, 39}}) {
        assert(worldToAxial(axialToWorld(c, kSize), kSize) == c);
    }
}

// Free movement: gliding straight up from neutral ground moves the avatar north
// (y decreases) and lays a multi-cell trail, without dying or leaving the board.
void testFreeMovementTrail() {
    HexWorld world(0, 21);
    soloHuman(world);
    world.placePlayerForTest(0, {20, 20}, HexDir::N); // far from the central home
    const Vec2 start = world.player().pos;
    for (int k = 0; k < 40; ++k) {
        world.setPlayerDesiredAngle(dirAngle(HexDir::N));
        world.step();
    }
    assert(world.playerAlive());
    assert(world.player().pos.y < start.y - 1.0F); // N points up (negative y)
    assert(world.player().cell != HexCoord(20, 20));
    assert(world.trailOf(0).size() >= 2);
    assert(world.grid().contains(world.player().cell));
}

// Steering straight into the left wall must never carry the avatar off the board
// or kill it — the heading deflects so it slides along the edge.
void testFreeMovementWallDeflect() {
    HexWorld world(0, 22);
    soloHuman(world);
    world.placePlayerForTest(0, {0, 28}, HexDir::NW); // NW would leave the board at q=0
    for (int k = 0; k < 40; ++k) {
        world.setPlayerDesiredAngle(dirAngle(HexDir::NW));
        world.step();
        assert(world.grid().contains(world.player().cell));
        assert(world.playerAlive());
    }
}

void testGrid() {
    const HexGrid g(10, 8);
    assert(g.width() == 10 && g.height() == 8 && g.cellCount() == 80);
    assert(g.contains({0, 0}) && g.contains({9, 7}));
    assert(!g.contains({-1, 0}) && !g.contains({10, 0}) && !g.contains({0, 8}));
    for (const HexCoord c : {HexCoord{0, 0}, HexCoord{9, 7}, HexCoord{3, 5}}) {
        assert(g.fromIndex(g.index(c)) == c);
    }
}

void testStartHome() {
    HexWorld world(0, 1234);
    // Home is a radius-2 hex blob: 1 + 6 + 12 = 19 cells, all owned by the human.
    assert(world.territoryCount(0) == 19);
    assert(world.playerAlive());
    assert(world.playerPercent() > 0.0F);
    assert(world.totalCells() == 56 * 56);
}

void testTrailRecording() {
    HexWorld world(0, 7);
    world.placePlayerForTest(0, {5, 5}, HexDir::N); // far from the central home
    assert(world.trailOf(0).empty());
    walk(world, HexDir::N);  // enter (5,4)
    walk(world, HexDir::NE); // enter (6,3)
    const auto& trail = world.trailOf(0);
    assert(trail.size() == 2);
    assert(world.trailOwnerAt({5, 4}) == 0);
    assert(world.trailOwnerAt({6, 3}) == 0);
    assert(world.playerAlive());
}

// Pre-own five of a center cell's six neighbors, then walk the trail across the
// sixth and back onto owned land. Closing the loop should claim the trail cell AND
// the now-sealed center cell.
void testEnclosureCapture() {
    HexWorld world(0, 99);
    const HexCoord x{15, 15};
    HexCoord ring[6];
    for (int d = 0; d < 6; ++d) {
        ring[d] = neighbor(x, static_cast<HexDir>(d));
    }
    for (int d = 0; d < 5; ++d) {
        world.setOwnerForTest(ring[d], 0); // R0..R4 owned; R5 is the gap
    }
    const HexCoord gap = ring[5];
    assert(world.ownerAt(x) != 0);
    assert(world.ownerAt(gap) != 0);

    world.placePlayerForTest(0, ring[4], dirBetween(ring[4], gap));
    walk(world, dirBetween(ring[4], gap)); // step onto the gap -> lay trail
    assert(world.ownerAt(gap) != 0);       // not captured yet, just trail
    walk(world, dirBetween(gap, ring[0])); // step back onto owned land -> close

    assert(world.ownerAt(gap) == 0); // trail became land
    assert(world.ownerAt(x) == 0);   // sealed center captured
    assert(world.playerAlive());
}

// A neutral cell that can still reach the map border is NOT captured.
void testOpenRegionNotCaptured() {
    HexWorld world(0, 5);
    const HexCoord x{15, 15};
    // Own only four neighbors, leaving two gaps — x stays connected to the outside.
    for (int d = 0; d < 4; ++d) {
        world.setOwnerForTest(neighbor(x, static_cast<HexDir>(d)), 0);
    }
    world.placePlayerForTest(0, neighbor(x, static_cast<HexDir>(3)), HexDir::N);
    // Lay a trivial trail and close it elsewhere; x must remain neutral.
    world.setOwnerForTest({40, 40}, 0);
    world.placePlayerForTest(0, {40, 40}, HexDir::N);
    walk(world, HexDir::N);                      // (40,39) trail
    walk(world, dirBetween({40, 39}, {40, 40})); // back home -> close
    assert(world.ownerAt(x) != 0);
}

void testSelfTrailDeath() {
    HexWorld world(0, 42);
    const HexCoord a{8, 8};
    world.placePlayerForTest(0, a, HexDir::N);
    walk(world, HexDir::N);  // enter b = (8,7), trail
    walk(world, HexDir::SE); // enter c = (9,7), trail
    walk(world, HexDir::SW); // enter a = (8,8), trail (back to start cell)
    assert(world.playerAlive());
    walk(world, HexDir::N); // enter b again -> own trail -> death
    assert(!world.playerAlive());
}

void testWallDeflection() {
    HexWorld world(0, 3);
    world.placePlayerForTest(0, {0, 10}, HexDir::NW); // NW leaves the board at q=0
    walk(world, HexDir::NW);
    // Deflected to the nearest in-bounds direction (N), still alive, still in bounds.
    assert(world.playerAlive());
    const HexCoord c = world.player().cell;
    assert(world.grid().contains(c));
    assert(c == HexCoord(0, 9));
}

void testCaptureConservesCount() {
    HexWorld world(0, 11);
    const int before = world.territoryCount(0);
    const HexCoord x{20, 20};
    HexCoord ring[6];
    for (int d = 0; d < 6; ++d) {
        ring[d] = neighbor(x, static_cast<HexDir>(d));
    }
    for (int d = 0; d < 5; ++d) {
        world.setOwnerForTest(ring[d], 0);
    }
    world.placePlayerForTest(0, ring[4], dirBetween(ring[4], ring[5]));
    walk(world, dirBetween(ring[4], ring[5]));
    walk(world, dirBetween(ring[5], ring[0]));
    // +5 pre-owned ring, +1 trail (gap), +1 enclosed center = +7.
    assert(world.territoryCount(0) == before + 7);
}

// Two worlds with the same difficulty + seed, stepped identically (no input),
// must stay byte-identical — bots, collisions, and respawns are all deterministic.
void testDeterminism() {
    HexWorld a(1, 777);
    HexWorld b(1, 777);
    for (int k = 0; k < 300; ++k) {
        a.step();
        b.step();
    }
    assert(a.players().size() == b.players().size());
    for (std::size_t i = 0; i < a.players().size(); ++i) {
        assert(a.players()[i].alive == b.players()[i].alive);
        assert(a.players()[i].cell == b.players()[i].cell);
        assert(a.players()[i].territoryCount == b.players()[i].territoryCount);
    }
    const auto& ca = a.grid().cells();
    const auto& cb = b.grid().cells();
    assert(ca.size() == cb.size());
    for (std::size_t i = 0; i < ca.size(); ++i) {
        assert(ca[i].owner == cb[i].owner);
        assert(ca[i].trailOwner == cb[i].trailOwner);
    }
}

// Stepping onto a rival's trail kills the rival (and clears their trail); the
// cutter survives.
void testTrailCutDeath() {
    HexWorld world(0, 123);
    const int n = static_cast<int>(world.players().size());
    for (int id = 2; id < n; ++id) {
        world.setAliveForTest(static_cast<og::hexanaut::PlayerId>(id), false);
    }
    // Clear the test corridor so ownership doesn't change the mechanics.
    for (const HexCoord c :
         {HexCoord{5, 5}, HexCoord{6, 5}, HexCoord{7, 5}, HexCoord{8, 5}, HexCoord{6, 4}}) {
        world.setOwnerForTest(c, og::hexanaut::kNeutral);
    }

    // Lay victim (id 1) trail while the human is parked.
    world.setAliveForTest(0, false);
    world.placePlayerForTest(1, {5, 5}, HexDir::SE);
    world.setDesiredDirForTest(1, HexDir::SE);
    world.advanceCellForTest(); // 1 -> (6,5)
    world.advanceCellForTest(); // 1 -> (7,5)
    assert(world.trailOf(1).size() == 2);

    // Human cuts the trail at (6,5).
    world.setAliveForTest(0, true);
    world.placePlayerForTest(0, {6, 4}, HexDir::S);
    world.setDesiredDirForTest(0, HexDir::S);
    world.setDesiredDirForTest(1, HexDir::SE);
    world.advanceCellForTest();

    assert(!world.players()[1].alive); // victim cut down
    assert(world.players()[0].alive);  // cutter survives
    assert(world.trailOf(1).empty());
    assert(world.trailOwnerAt({7, 5}) == og::hexanaut::kNoTrail);
}

// Cutting a rival's trail hands the cutter the victim's whole territory rather
// than freeing it to neutral.
void testTrailCutCapturesTerritory() {
    HexWorld world(0, 123);
    const int n = static_cast<int>(world.players().size());
    for (int id = 2; id < n; ++id) {
        world.setAliveForTest(static_cast<og::hexanaut::PlayerId>(id), false);
    }
    for (const HexCoord c :
         {HexCoord{5, 5}, HexCoord{6, 5}, HexCoord{7, 5}, HexCoord{8, 5}, HexCoord{6, 4}}) {
        world.setOwnerForTest(c, og::hexanaut::kNeutral);
    }

    // Lay victim (id 1) trail while the human is parked.
    world.setAliveForTest(0, false);
    world.placePlayerForTest(1, {5, 5}, HexDir::SE);
    world.setDesiredDirForTest(1, HexDir::SE);
    world.advanceCellForTest(); // 1 -> (6,5)
    world.advanceCellForTest(); // 1 -> (7,5)

    const int before0 = world.territoryCount(0);
    const int before1 = world.territoryCount(1);
    assert(before1 > 0);
    const HexCoord victimHome = world.players()[1].home;
    assert(world.ownerAt(victimHome) == 1);

    // Human cuts the trail at (6,5) -> victim dies and the human inherits its land.
    world.setAliveForTest(0, true);
    world.placePlayerForTest(0, {6, 4}, HexDir::S);
    world.setDesiredDirForTest(0, HexDir::S);
    world.setDesiredDirForTest(1, HexDir::SE);
    world.advanceCellForTest();

    assert(!world.players()[1].alive);
    assert(world.territoryCount(1) == 0);
    assert(world.territoryCount(0) == before0 + before1); // cutter annexed the territory
    assert(world.ownerAt(victimHome) == 0);               // victim's cells are now the cutter's
}

// Two players moving into the same cell on the same tick both fall.
void testHeadToHead() {
    HexWorld world(0, 55);
    const int n = static_cast<int>(world.players().size());
    for (int id = 2; id < n; ++id) {
        world.setAliveForTest(static_cast<og::hexanaut::PlayerId>(id), false);
    }
    world.placePlayerForTest(0, {5, 5}, HexDir::SE); // (5,5) -> SE -> (6,5)
    world.placePlayerForTest(1, {7, 5}, HexDir::NW); // (7,5) -> NW -> (6,5)
    world.setDesiredDirForTest(0, HexDir::SE);
    world.setDesiredDirForTest(1, HexDir::NW);
    world.advanceCellForTest();
    assert(!world.players()[0].alive);
    assert(!world.players()[1].alive);
}

// A captured shooter blooms its owner's territory outward, capturing the nearest
// un-owned cells one at a time until the whole disc within range is claimed.
void testShooterCaptures() {
    HexWorld world(0, 7);
    soloHuman(world);
    const HexCoord s{20, 20}; // far from the central home; surrounded by neutral ground
    world.setOwnerForTest(s, 0);
    for (int d = 0; d < 6; ++d) {
        assert(world.ownerAt(neighbor(s, static_cast<HexDir>(d))) != 0); // neutral to begin
    }
    const int before = world.territoryCount(0);

    world.setShooterForTest(s);
    world.advanceShootersForTest(3000); // many capture cycles (slows as the disc grows)

    // The full first ring is claimed, and the shooter has annexed a sizeable disc.
    for (int d = 0; d < 6; ++d) {
        assert(world.ownerAt(neighbor(s, static_cast<HexDir>(d))) == 0);
    }
    assert(world.territoryCount(0) >= before + 18);
}

// An un-captured shooter (its cell is neutral) does nothing — no captures occur.
void testShooterInertWhenUnowned() {
    HexWorld world(0, 7);
    soloHuman(world);
    const HexCoord s{30, 30};
    assert(world.ownerAt(s) == og::hexanaut::kNeutral);
    const int before = world.territoryCount(0);
    world.setShooterForTest(s);
    world.advanceShootersForTest(500);
    assert(world.territoryCount(0) == before); // neutral shooter never fires
}

// Stepping onto a Speed power-up lowers stepInterval and arms its timer; a Vision
// power-up arms the vision timer.
void testPowerupPickup() {
    HexWorld world(0, 8);
    const int n = static_cast<int>(world.players().size());
    for (int id = 1; id < n; ++id) {
        world.setAliveForTest(static_cast<og::hexanaut::PlayerId>(id), false);
    }
    for (const HexCoord c : {HexCoord{5, 5}, HexCoord{5, 4}, HexCoord{5, 3}}) {
        world.setOwnerForTest(c, og::hexanaut::kNeutral);
    }
    world.placePlayerForTest(0, {5, 5}, HexDir::N);
    const float base = world.players()[0].stepInterval;

    world.setPowerupForTest({5, 4}, og::hexanaut::PowerUp::Speed);
    world.setDesiredDirForTest(0, HexDir::N);
    world.advanceCellForTest(); // enter (5,4) -> Speed
    assert(world.players()[0].stepInterval < base);
    assert(world.players()[0].speedTimer > 0.0F);

    world.setPowerupForTest({5, 3}, og::hexanaut::PowerUp::Vision);
    world.setDesiredDirForTest(0, HexDir::N);
    world.advanceCellForTest(); // enter (5,3) -> Vision
    assert(world.players()[0].visionTimer > 0.0F);
}

} // namespace

int main() {
    testHexMath();
    testWorldToAxialRoundTrip();
    testFreeMovementTrail();
    testFreeMovementWallDeflect();
    testGrid();
    testStartHome();
    testTrailRecording();
    testEnclosureCapture();
    testOpenRegionNotCaptured();
    testSelfTrailDeath();
    testWallDeflection();
    testCaptureConservesCount();
    testDeterminism();
    testTrailCutDeath();
    testTrailCutCapturesTerritory();
    testShooterCaptures();
    testShooterInertWhenUnowned();
    testHeadToHead();
    testPowerupPickup();
    std::puts("All Hexanaut tests passed.");
    return 0;
}
