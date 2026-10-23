#pragma once

#include "city.hpp"
#include "tour.hpp"

#include <cstddef>
#include <random>
#include <string>
#include <tuple>
#include <vector>

void run_ipc_server(const std::size_t MAX_GEN_COUNT = 350);

namespace Tsp::Utils {

std::string trim(const std::string &value);

void launch_frontend(const unsigned short port_num = 56767);

}  // Closing namespace Tsp::Utils

namespace Tsp {

// Returns the total distance of a tour represented by a state
double tour_len(const Tour &tour, const Cities &cities);

Cities load_cities(const size_t &NUM_CITIES = 350);

std::size_t receive_data(void *contents, std::size_t size, std::size_t count, void *output_buffer);

std::string cities_to_json(const Cities &cities);
std::string tour_to_json(const Tour &tour);

}  // Closing namespace Tsp

namespace Tsp::Ga {

Tour greedy_tour(const Cities &cities, std::mt19937_64 &rng);
void improve_tour(Tour &tour, const Cities &cities);
Tours build_init_pop(const Cities &cities, std::mt19937_64 &rng,
    const std::size_t &POP_SIZE = 200);
Tour rand_tour(std::mt19937_64 &rng, const int N);
double fitness(const Tour &tour, const Cities &cities);
std::size_t tourney_select(const Tours &pop, const Cities &cities,
    const std::vector<double> &fitness_scores, std::mt19937_64 &rng, const int NUM_CANDS = 3);
std::tuple<Tours, std::vector<double>, std::mt19937_64> init_ga(const Cities &cities);
Tour run_gen(Tours &pop, std::vector<double> &fitness_scores, std::mt19937_64 &rng, const Cities &cities);

}  // Closing namespace Tsp::Ga
