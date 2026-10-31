#pragma once

#include <cstddef>

namespace Tsp::Config {

inline constexpr std::size_t NUM_CITIES = 350;

inline constexpr std::size_t POP_SIZE = 200;

// Several children a parent-pair, keeping the shortest (best)
inline constexpr int NUM_CHILDREN = 10;

inline constexpr std::size_t MAX_GEN_COUNT = 1000;
inline constexpr std::size_t MAX_STAGNANT_GENS = 40;

}  // namespace Tsp::Config