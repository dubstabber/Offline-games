// Generate assets/solitaire/deals.bytes: per difficulty tier, a list of deal
// seeds that KlondikeSolver proves winnable under that tier's rules (Easy draw
// one, Medium draw three, Hard draw three with three passes). Seeds are tried in
// order from 1 and each tier's list is sorted cheapest-to-solve first.
//
//   cmake --build build/release --target gen_solitaire_deals
//   ./build/release/gen_solitaire_deals assets/solitaire/deals.bytes [perTier] [budget]
#include "games/solitaire/KlondikeGame.hpp"
#include "games/solitaire/KlondikeSolver.hpp"
#include "games/solitaire/SolitaireDeals.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct Found {
    std::uint32_t seed;
    int nodes;
};

std::vector<Found> collect(int tier, int wanted, int budget) {
    std::vector<Found> found;
    std::mutex mutex;
    std::atomic<std::uint32_t> nextSeed{1};
    std::atomic<int> tried{0};
    const auto worker = [&] {
        while (true) {
            {
                const std::lock_guard<std::mutex> lock(mutex);
                if (static_cast<int>(found.size()) >= wanted) {
                    return;
                }
            }
            const std::uint32_t seed = nextSeed++;
            const og::KlondikeGame game(og::klondikeRules(tier), seed);
            const og::KlondikeSolver::Result r = og::KlondikeSolver::solve(game, budget);
            ++tried;
            if (r.solved) {
                const std::lock_guard<std::mutex> lock(mutex);
                found.push_back(Found{.seed = seed, .nodes = r.nodes});
            }
        }
    };
    std::vector<std::thread> threads;
    const unsigned n = std::max(1U, std::thread::hardware_concurrency());
    for (unsigned i = 0; i < n; ++i) {
        threads.emplace_back(worker);
    }
    for (std::thread& t : threads) {
        t.join();
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) {
        return a.nodes != b.nodes ? a.nodes < b.nodes : a.seed < b.seed;
    });
    if (static_cast<int>(found.size()) > wanted) {
        found.resize(static_cast<std::size_t>(wanted));
    }
    std::printf("tier %d: %zu winnable seeds from %d deals tried (budget %d nodes)\n", tier,
                found.size(), tried.load(), budget);
    return found;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <out.bytes> [perTier=300] [budget=300000]\n", argv[0]);
        return 1;
    }
    const int perTier = argc > 2 ? std::atoi(argv[2]) : 300;
    const int budget = argc > 3 ? std::atoi(argv[3]) : 300000;
    og::SolitaireDealLists lists;
    for (int tier = 0; tier < og::kSolitaireTierCount; ++tier) {
        for (const Found& f : collect(tier, perTier, budget)) {
            lists.at(static_cast<std::size_t>(tier)).push_back(f.seed);
        }
    }
    const std::vector<std::uint8_t> bytes = og::serializeSolitaireDeals(lists);
    std::ofstream out(argv[1], std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    std::printf("wrote %zu bytes to %s\n", bytes.size(), argv[1]);
    return out ? 0 : 1;
}
