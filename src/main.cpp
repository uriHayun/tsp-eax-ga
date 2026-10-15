#define NOMINMAX

#include "city.hpp"
#include "crossover.hpp"
#include "distance.hpp"
#include "main.hpp"
#include "tour.hpp"

#include <algorithm>
#include <asio.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <curl/curl.h>
#include <dotenv/dotenv.hpp>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <numeric>
#include <optional>
#include <print>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <whereami++.hpp>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#if defined(__APPLE__)
    // Required to resume with TARGET_OS_OSX (macOS)
    #include <TargetConditionals.h>
#endif

using Json = nlohmann::json;

int main() {
    // Load environment variables from a local .env file into process environment
    dotenv::load(".env");
}

// Transfers the cities array once, and a tour each generation to Lua LÖVE2D in order to draw later
// using a standalone Asio TCP socket as an IPC local server connection
void run_ipc_server(const std::size_t MAX_GEN_COUNT = 1000) {
    using namespace Tsp;
    using namespace Tsp::Ga;
    using tcp = asio::ip::tcp;

    asio::io_context io;

    // Declare variables at top scope, so they survive try-catch blocks
    // Note: std::optional acts as a pointer
    std::optional<tcp::acceptor> acceptor;
    tcp::socket socket(io);
    std::optional<Cities> cities;

    try {
        acceptor.emplace(io, tcp::endpoint(tcp::v6(), 0));

        // Allow IPv4 connections as well as IPv6
        acceptor->set_option(asio::ip::v6_only(false));

        // Actual dynamic, private port number from the OS
        const unsigned short port_num = acceptor->local_endpoint().port();

        std::println("IPC server connection listening on port: {}", port_num);

        Utils::launch_frontend(port_num);
    }
    catch (const asio::system_error &e) {
        std::println(std::cerr, "Network initialization failed: {}", e.what());
        return;
    }
    catch (const std::exception &e) {
        std::println(std::cerr, "Failed to launch LÖVE2D frontend: {}", e.what());
        return;
    }

    try {
        bool connected = false;
        std::error_code accept_ec;

        acceptor->async_accept(socket, [&](const std::error_code &ec) {
            accept_ec = ec;
            connected = true;
        });

        // Block execution until LÖVE2D connects
        constexpr std::chrono::seconds LOVE2D_CONN_TIMEOUT(10);
        io.run_for(LOVE2D_CONN_TIMEOUT);

        if (!connected) {
            acceptor->cancel();
            throw std::runtime_error(std::format("LÖVE2D didn't connect in time ({})", LOVE2D_CONN_TIMEOUT.count()));
        }

        if (accept_ec) {
            throw std::system_error(accept_ec, "Failed to accept connection from LÖVE2D");
        }

        std::println("Connected!");

        // Disable Nagle's algorithm so tours are sent immediately instead of being buffered
        socket.set_option(tcp::no_delay(true));

        // Build & send once the cities array to LÖVE2D
        cities = load_cities();
        asio::write(socket, asio::buffer(cities_to_json(*cities)));
    }
    catch (const std::system_error &e) {
        std::println(std::cerr, "TCP handshake failed: {}", e.what());
        return;
    }
    catch (const std::exception &e) {
        std::println(std::cerr, "Failed to load cities: {}", e.what());
        return;
    }

    try {
        auto [pop, fitness_scores, rng] = init_ga(*cities);

        std::size_t gen_count = 0;
        double last_tour_len = std::numeric_limits<double>::max();

        // Temporary end-condition, later will benchmark/further check
        while (gen_count < MAX_GEN_COUNT) {
            Tour curr_tour = run_gen(pop, fitness_scores, rng, *cities);
            gen_count++;

            double curr_tour_len = tour_len(curr_tour, *cities);

            if (curr_tour_len < last_tour_len) {
                last_tour_len = curr_tour_len;
            }

            // Send the chosen tour to LÖVE2D to draw each generation
            asio::write(socket, asio::buffer(tour_to_json(curr_tour)));
        }
    }
    catch (const asio::system_error &e) {
        std::println(std::cerr, "LÖVE2D disconnected mid-simulation: {}", e.what());
    }
}

namespace Tsp::Utils {

// Removes leading and trailing whitespace from a string
std::string trim(const std::string &value) {
    return boost::trim_copy_if(value, boost::is_any_of(" \t\r\n"));
}

// Launches LÖVE2D window with a port connection number argument
// (supports all widely-used OSs: Windows, macOS, Linux based OSs (e.g., ChromeOS))
void launch_frontend(const unsigned short PORT_NUM) {
    using path = std::filesystem::path;

    path love2d_path;
    path bin_dir = path(whereami::executable_path()).parent_path();  // Program folder
    path lua_dir = bin_dir / "lua";  // Lua folder

    auto love2d_path_exists = [&]() {
        if (!std::filesystem::exists(love2d_path)) {
            throw std::runtime_error(std::format("Failed to launch LÖVE2D frontend: {} not found",
                love2d_path.string()));
        }
    };

    // Launch LÖVE2D for Windows using Win32 API
    #if defined(_WIN32)
        love2d_path = bin_dir / "love.exe";
        love2d_path_exists();

        // std::wstring, non-constant string required for .data() for CreateProcessW() (launch function)
        std::wstring cmd = std::format(L"\"{}\" \"{}\" {}",
            love2d_path.wstring(),
            lua_dir.wstring(),
            std::to_wstring(PORT_NUM));  // Gets loaded as "arg" in love.load(arg)

        // Windows API's CreateProcessW() (launch function) configuration
        STARTUPINFOW si = { sizeof(si) };  // Default configuration for window - LÖVE2D configures its own window
        PROCESS_INFORMATION pi;           // Empty container to receive LÖVE2D process info from Windows
        ZeroMemory(&pi, sizeof(pi));      // Remove garbage data from RAM to prevent bugs & crashes

        if (!CreateProcessW(
            nullptr,     // Omitted app name
            cmd.data(),  // Launch command <LÖVE2D path, Lua directory, arg>
            nullptr,     // Main container cannot be inherited by child processes
            nullptr,     // Execution engine cannot be inherited by child processes
            FALSE,       // Starts fresh - no inherited handles
            0,           // Opens as a normal desktop window with no special flags
            nullptr,     // Uses same environment variables as this process (C++ backend)
            nullptr,     // Uses same working directory as this process (C++ backend)
            &si,         // Startup configuration for LÖVE2D process
            &pi          // Receives LÖVE2D process info from Windows
        )) {
            throw std::runtime_error(std::format("Failed to launch LÖVE2D frontend: {}", GetLastError()));
        }

        // Clean up handles - prevent memory leaks
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

    // Launch LÖVE2D window for macOS & Linux based OSs
    #elif TARGET_OS_OSX || defined(__linux__)
        love2d_path = bin_dir / "love";
        love2d_path_exists();

        pid_t pid = fork();

        // Failed to launch LÖVE2D frontend - couldn't create a new child process
        if (pid < 0) {
            throw std::runtime_error("Failed to launch LÖVE2D frontend: couldn't create a new child process");
        }

        // LÖVE2D is the child process - C++ backend can compute more tours 
        // (while LÖVE2D draws the previous tour)
        if (pid != 0) {
            return;
        }

        // Create LÖVE2D child process which runs simultaneously with the C++ backend
        execl(love2d_path.c_str(),               // Program path to launch (LÖVE2D)
              love2d_path.c_str(),               // argv[0]: program name (LÖVE2D)
              lua_dir.c_str(),                   // argv[1]: Lua directory folder
              std::to_string(PORT_NUM).c_str(),  // argv[2]: port number argument - gets loaded in "arg" in love.load(arg)
              static_cast<char *>(nullptr));     // Terminate argument list

        // Terminate child process - reached only if execl() fails
        _exit(EXIT_FAILURE);

    // Unsupported OS - not Windows, macOS, or Linux based
    #else
        throw std::runtime_error("Failed to launch LÖVE2D frontend: unsupported OS");
    #endif
}

}  // Closing namespace Tsp::Utils

namespace Tsp {

// Returns the total distance of a tour represented by a state
double tour_len(const Tour &tour, const Cities &cities) {
    double total_dist = 0.0;

    for (std::size_t i = 0, N = tour.size(); i < N; i++) {
            // In cities[tour[(i + 1) % tour.size()]] (from-city) wrap around to first 
            // after last city using modulo operator
            total_dist += edge_len({ tour[(i + 1) % tour.size()], tour[i] }, 
                cities);
    }

    return total_dist;
}

//
Cities load_cities(const std::size_t &NUM_CITIES) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        // HTTP request failed
        throw std::runtime_error("Failed to initialize CURL\n");
    }

    std::string response;

    const std::string GEONAMES_USERNAME = dotenv::get<std::string>("GEONAMES_USERNAME").value_or("");
    if (GEONAMES_USERNAME.empty()) {
        curl_easy_cleanup(curl);

        throw std::runtime_error("GEONAMES_USERNAME environment variable is not set\n");
    }

    const std::string URL = std::format("https://secure.geonames.org/searchJSON"
        "?country=US&featureClass=P&maxRows={}&username={}", NUM_CITIES, GEONAMES_USERNAME);

    curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Follow HTTP redirects (301, 302) automatically
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Prevent the request from hanging indefinitely by limiting the connection and total timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        curl_easy_cleanup(curl);

        throw std::runtime_error("Failed to perform CURL request\n");
    }

    long http_status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_status);
    if (http_status != 200) {
        curl_easy_cleanup(curl);

        throw std::runtime_error(std::format("HTTP request failed with status code: {}\n", http_status));
    }

    if (response.empty()) {
        curl_easy_cleanup(curl);

        throw std::runtime_error("Received empty response\n");
    }

    curl_easy_cleanup(curl);

    Cities cities;

    try {
        Json data = Json::parse(response);
        if (!data.contains("geonames") || !data["geonames"].is_array()) {
            throw std::runtime_error(R"(Invalid response format:
                "geonames" key not found or is not an array\n)");
        }

        for (const auto &city : data["geonames"]) {
            cities.push_back({
                std::stod(city["lat"].get<std::string>()),
                std::stod(city["lng"].get<std::string>())
            });
        }

        // Print longitude and latitude - just for verification for now
        for (const City &city : cities) {
            std::println("City: latitude={}, longitude={}", city.lat, city.lng);
        }
    }
    catch (const Json::parse_error &e) {
        throw std::runtime_error(std::format("Failed to parse JSON response: {}\n", e.what()));
    }
    catch (Json::exception &e) {
        throw std::runtime_error(std::format("JSON exception occurred: {}\n", e.what()));
    }

    return cities;
}

// libcurl callback to append HTTP received response buffers data to a string
// Note: libcurl is a C library, so this callback uses raw pointers instead of C++ references
std::size_t receive_data(void *contents, std::size_t size, std::size_t count, void *output_buffer) {
    std::size_t total_size = size * count;
    std::string *res = static_cast<std::string *>(output_buffer);
    res->append(static_cast<char *>(contents), total_size);

    return total_size;
}

//
std::string cities_to_json(const Cities &cities) {
    Json json;

    for (const City &city : cities) {
        json["cities"].push_back({ city.lat, city.lng });
    }

    return json.dump() + '\n';
}

std::string tour_to_json(const Tour &tour) {
    Json json;

    json["tour"] = tour;

    return json.dump() + '\n';
}

}  // Closing namespace Tsp

namespace Tsp::Ga {

// Builds a single tour derived from cities by greedily 
// getting the nearest unvisited each iteration to be the next city
Tour greedy_tour(const Cities &cities, std::mt19937_64 &rng) {
    std::size_t num_cities = cities.size();
    std::vector<bool> visited(num_cities, false);

    // Get a random start city
    std::uniform_int_distribution<std::size_t> distrib(0, num_cities - 1);
    std::size_t start_city_idx = distrib(rng);

    // Initialize tour with the random start city
    Tour tour;
    tour.reserve(num_cities);
    tour.push_back(start_city_idx);
    visited[start_city_idx] = true;

    for (std::size_t i = 1; i < num_cities; i++) {
        std::size_t curr_city_idx = tour.back();

        double best_dist = std::numeric_limits<double>::infinity();
        std::size_t nearest_city_idx = 0;
        bool found_next = false;

        // Find next unused nearest city
        for (std::size_t cand_city_idx = 0; cand_city_idx < num_cities; cand_city_idx++) {
            if (visited[cand_city_idx]) {
                continue;
            }

            double dist = haversine_distance(cities[curr_city_idx], cities[cand_city_idx]);

            if (dist < best_dist) {
                best_dist = dist;
                nearest_city_idx = cand_city_idx;
                found_next = true;
            }
        }

        // Sanity check: was a next city found?
        assert(found_next && "No unvisited city found");

        tour.push_back(nearest_city_idx);
        visited[nearest_city_idx] = true;
    }

    return tour;
}

// Improves a tour using a search heuristic: repeatedly finds a pair of edges whose 
// swapping shortens the tour, until it finds no improving swap left
void improve_tour(Tour &tour, const Cities &cities) {
    const std::size_t num_cities = cities.size();
    bool improved = true;

    // Keep until no swap improvement is left/found
    while (improved) {
        improved = false;

        for (std::size_t i = 0; i < num_cities - 1; i++) {
            for (std::size_t j = i + 1; j < num_cities; j++) {
                // Edge 1's cities
                std::size_t edge1_a = tour[i];
                std::size_t edge1_b = tour[(i + 1) % num_cities];

                // Edges 2's cities
                std::size_t edge2_a = tour[j];
                std::size_t edge2_b = tour[(j + 1) % num_cities];

                const double before_dist = haversine_distance(cities[edge1_a], cities[edge1_b])
                    + haversine_distance(cities[edge2_a], cities[edge2_b]);

                const double after_dist = haversine_distance(cities[edge1_a], cities[edge2_a])
                    + haversine_distance(cities[edge1_b], cities[edge2_b]);

                if (after_dist < before_dist) {
                    std::ranges::reverse(tour.begin() + i + 1, tour.begin() + j + 1);
                    improved = true;
                }
            }
        }
    }
}

// Builds initial population of POP_SIZE tours,
// each constructed greedily then improved with a search heuristic
Tours build_init_pop(const Cities &cities, std::mt19937_64 &rng,
    const std::size_t &POP_SIZE) {
    Tours pop;
    pop.reserve(POP_SIZE);

    for (std::size_t i = 0 ; i < POP_SIZE; i++) {
        Tour tour = greedy_tour(cities, rng);
        improve_tour(tour, cities);
        pop.push_back(std::move(tour));
    }

    return pop;
}

// Returns a random unsorted sequence of integers 0 to N-1
Tour rand_tour(std::mt19937_64 &rng, const int N) {
    // Create a state with N (default 50) ordered cities (0 to N-1)
    Tour tour(N);
    std::ranges::iota(tour, 0);

    // Shuffle the ordered state to create a random state
    std::ranges::shuffle(tour, rng);

    return tour;
}

// Converts tour's distance into a fitness score, the higher the score the better it is
double fitness(const Tour &tour, const Cities &cities) {
    double dist = tour_len(tour, cities);

    // Fitness score is inversely proportional to distance
    // Add a small epsilon to avoid division by 0 (0 distance edge case)
    return 1.0 / (dist + 1e-9);
}

// Selects the shortest (best) tour from a set of NUM_CANDS candidates
// using tournament selection
std::size_t tourney_select(const Tours &pop, const Cities &cities,
    const std::vector<double> &fitness_scores, std::mt19937_64 &rng, const int NUM_CANDS) {
    assert(!pop.empty());

    std::uniform_int_distribution<std::size_t> distrib(0, pop.size() - 1);
    std::size_t best_cand_idx = distrib(rng);
    double best_score = fitness_scores[best_cand_idx];

    for (int i = 0; i < NUM_CANDS - 1; i++) {
        const std::size_t cand_idx = distrib(rng);
        const double cand_score = fitness_scores[cand_idx];

        if (best_score < cand_score) {
            // Current candidate becomes the best candidate
            best_cand_idx = cand_idx;
            best_score = cand_score;
        }
    }

    return best_cand_idx;  // Best candidate from tournament
}

// TODO: comment here
std::tuple<Tours, std::vector<double>, std::mt19937_64> init_ga(const Cities &cities) {
    std::random_device rd;
    std::mt19937_64 rng(rd());

    Tours pop = build_init_pop(cities, rng);

    std::vector<double> fitness_scores;
    fitness_scores.reserve(pop.size());

    for (const Tour &tour : pop) {
        fitness_scores.push_back(fitness(tour, cities));
    }

    return { std::move(pop), std::move(fitness_scores), std::move(rng) };
}

// the GA's main procedure:
// TODO: explain GA's procedure: start -> initialization -> crossover -> (bc of EAX there's no mutation) 
// -> fitness (evaluation) -> selection -> end-condition (temporary) -> end
Tour run_gen(Tours &pop, std::vector<double> &fitness_scores, std::mt19937_64 &rng, const Cities &cities) {
    using Eax::init_crossover;
    using Eax::crossover;

    const std::size_t parent_a_idx = tourney_select(pop, cities, fitness_scores, rng);
    const std::size_t parent_b_idx = tourney_select(pop, cities, fitness_scores, rng);

    auto [edges_a, cycles] = init_crossover(pop[parent_a_idx], pop[parent_b_idx]);
    const Tour offspring = crossover(cycles, edges_a, cities);

    if (tour_len(offspring, cities) > tour_len(pop[parent_a_idx], cities)) {
        return pop[parent_a_idx];
    }

    pop[parent_a_idx] = offspring;
    fitness_scores[parent_a_idx] = fitness(offspring, cities);

    return offspring;
}

}  // Closing namespace Tsp::Ga