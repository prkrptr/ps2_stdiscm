#include <iostream>
#include <filesystem>
#include <queue>          // For player queues
#include <thread>
#include <mutex>
#include <condition_variable> // For efficient waiting
#include <semaphore>      // For limiting concurrent instances (C++20)
#include <chrono>         // For time durations
#include <random>         // For random dungeon clear times
#include <atomic>         // For the monitoring thread exit condition
#include <algorithm>
#include <memory>
#include "DungeonInstance.h"

// global variables
//n - maximum number of concurrent instances
int n_instances = 0;
// t - number of tank players in the queue
int tank = 0;
// h - number of healer players in the queue
int healer = 0;
// d - number of DPS players in the queue
int dps= 0;
// t1 - minimum time before an instance is finished
int min_time = 0;
// t2 - maximum time before an instance is finished
int max_time = 0;

std::vector<std::unique_ptr<DungeonInstance>> dungeons;
std::queue<int> tank_queue;
std::queue<int> healer_queue;
std::queue<int> dps_queue;

// for synchronizing
std::mutex queue_mutex;
std::mutex cout_mutex;
std::condition_variable party_condition; // for dispatcher to wait
std::counting_semaphore<> dungeon_slots(0); //limits concurrent dungeons
std::atomic<bool> simulation_running = true; // flag to control monitor thread

void log_message(const std::string & basic_string);

void run_dungeon(int instance_id) {
    DungeonInstance& dungeon = *dungeons[instance_id];

    dungeon.is_active = true;

    log_message("[Party] A party has entered dungeon " + std::to_string(dungeon.instance_id) + ".");


    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min_time, max_time > min_time ? max_time : min_time);
    int run_time_sec = distrib(gen);

    std::this_thread::sleep_for(std::chrono::seconds(run_time_sec));

    //update stats and announce exit
    dungeon.is_active = false;
    dungeon.parties_served++;
    dungeon.total_time_served += run_time_sec;

    log_message("[Party] Party in instance " + std::to_string(dungeon.instance_id) + " finished in " + std::to_string(run_time_sec) + "s. Slot is now free.");

    dungeon_slots.release(); // release the semaphore slot

    party_condition.notify_one(); // notifies the dispatcher in case it's waiting
}


//bonus
void player_producer_thread() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> role_distrib(0, 4);  // 0=Tank, 1=Healer, 2,3,4=DPS (to make DPS more common)
    std::uniform_int_distribution<> time_distrib(3, 10); // New player every 3-10 seconds

    while (simulation_running) {
        // sleep for a random amount of time
        std::this_thread::sleep_for(std::chrono::seconds(time_distrib(gen)));

        // lock the queues to safely add a new player
        std::lock_guard<std::mutex> lock(queue_mutex);

        int role = role_distrib(gen);
        std::string role_name;
        if (role == 0) {
            tank_queue.push(0);
            role_name = "TANK";
        } else if (role == 1) {
            healer_queue.push(0);
            role_name = "HEALER";
        } else {
            dps_queue.push(0);
            role_name = "DPS";
        }

        // Announce the new player
        log_message("[Producer] A new " + role_name + " has joined the queue!");


        // Notify the dispatcher that a new player has arrived, potentially unblocking its wait condition.
        party_condition.notify_one();
    }
}

// dispatcher logic for BONUS mode
void dispatcher_thread() {
    int total_parties_possible = std::min({tank, healer, dps / 3});
    log_message("[Dispatcher] Phase 1 starting. Processing initial " + std::to_string(total_parties_possible) + " parties.");

    for (int parties_formed = 0; parties_formed < total_parties_possible; ++parties_formed) {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // wait for players (this should be immediate for the initial set)
        party_condition.wait(lock, [] {
            return tank_queue.size() >= 1 && healer_queue.size() >= 1 && dps_queue.size() >= 3;
        });

        // form the party
        tank_queue.pop(); healer_queue.pop(); dps_queue.pop(); dps_queue.pop(); dps_queue.pop();
        lock.unlock();

        log_message("[Dispatcher] Party formed! Waiting for an available dungeon...");
        dungeon_slots.acquire();

        // launch the party thread
        for (auto& dungeon_ptr : dungeons) {
            if (!dungeon_ptr->is_active) {
                std::thread(run_dungeon, dungeon_ptr->instance_id).detach();
                break;
            }
        }
    }

    log_message("[Dispatcher] Phase 1 complete. All initial players have been dispatched.");

    // start bonus thread
    log_message("[System] Activating Bonus Mode: New players will now join the queue randomly.");
    std::thread producer(player_producer_thread);
    producer.detach(); // detach the producer so we dont have to join it

    // bonus - produce players
    while (simulation_running) {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Wait for new players from the producer OR for the simulation to end
        party_condition.wait(lock, [] {
            return (!simulation_running) || (tank_queue.size() >= 1 && healer_queue.size() >= 1 && dps_queue.size() >= 3);
        });

        if (!simulation_running) break;
        if (tank_queue.size() < 1 || healer_queue.size() < 1 || dps_queue.size() < 3) continue;

        // Form a party from the newly arrived players
        tank_queue.pop(); healer_queue.pop(); dps_queue.pop(); dps_queue.pop(); dps_queue.pop();
        lock.unlock();

        log_message("[Dispatcher] Party formed from new players! Waiting for an available dungeon...");
        dungeon_slots.acquire();

        if (!simulation_running) {
            dungeon_slots.release();
            break;
        }

        for (auto& dungeon_ptr : dungeons) {
            if (!dungeon_ptr->is_active) {
                std::thread(run_dungeon, dungeon_ptr->instance_id).detach();
                break;
            }
        }
    }
}

// show status of instances and queues
void monitor_thread() {
    while (simulation_running) {
        // ADD this line to clear the screen for a cleaner UI
        std::cout << "\033[2J\033[1;1H";

        std::cout << "--- LFG Dungeon Simulator ---" << std::endl;
        std::cout << "==========================================" << std::endl;
        std::cout << "Dungeon Instances (" << n_instances << " total):" << std::endl;

        for (const auto& dungeon_ptr : dungeons) {
            std::cout << "  Instance " << dungeon_ptr -> instance_id << ":\t"
                      << (dungeon_ptr->is_active ? "Active" : "Empty") << std::endl;
        }

        std::cout << "\nPlayer Queues:" << std::endl;
        {
            std::lock_guard<std::mutex> lock(queue_mutex); // safely read queue sizes
            std::cout << "  Tanks:   " << tank_queue.size() << std::endl;
            std::cout << "  Healers: " << healer_queue.size() << std::endl;
            std::cout << "  DPS:     " << dps_queue.size() << std::endl;
        }
        std::cout << "==========================================" << std::endl;

        std::cout << "(Screen refreshes every second. Press Enter to stop.)" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
int get_validated_input(const std::string& prompt, int min_val, int max_val) {
    int value;
    std::string line;

    while (true) {
        std::cout << prompt;
        std::getline(std::cin, line); // read the entire line to handle extra characters

        try {
            size_t pos;
            value = std::stoi(line, &pos);

            //  catches inputs like "123xyz".
            if (pos != line.length()) {
                throw std::invalid_argument("Trailing characters found.");
            }

            // Check if the value is within the logical range
            if (value >= min_val && value <= max_val) {
                return value; // success
            } else {
                std::cerr << "Error: Input must be between " << min_val << " and " << max_val << ". Please try again.\n";
            }
        } catch (const std::invalid_argument&) {
            // Handles cases where input is not a number at all, e.g., "abc"
            std::cerr << "Error: Invalid input. Please enter a valid whole number.\n";
        } catch (const std::out_of_range&) {
            // Handles cases where the number is too big or small for an 'int'
            std::cerr << "Error: Number is too large or too small. Please try again.\n";
        }
    }
}

std::string get_current_timestamp_ms() {
    auto now = std::chrono::system_clock::now();

    // Separate the seconds and the fractional part (milliseconds)
    auto seconds_since_epoch = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto fractional_part = now - seconds_since_epoch;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(fractional_part);

    // Convert seconds to a time_t
    std::time_t in_time_t = std::chrono::system_clock::to_time_t(now);

    // Use thread-safe versions of localtime
    std::tm buf;
#ifdef _WIN32
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf); // POSIX-compliant
#endif

    // Format the string using a stringstream
    std::stringstream ss;
    ss << std::put_time(&buf, "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << milliseconds.count();

    return ss.str();
}


void log_message(const std::string& message) {
    // lock the mutex to ensure the timestamp and message are printed together without interruption
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[" << get_current_timestamp_ms() << "] " << message << std::endl;
}

void inputMode() {
    const int MAX_VAL = std::numeric_limits<int>::max();

    std::cout << "\nWELCOME TO LFG (Looking for Group) dungeon Simulator" << std::endl;

    // Get validated inputs using the helper function
    n_instances = get_validated_input("Enter Max number of concurrent instances: ", 1, MAX_VAL);
    tank = get_validated_input("Enter number of tank players: ", 0, MAX_VAL);
    healer = get_validated_input("Enter number of healer players: ", 0, MAX_VAL);
    dps = get_validated_input("Enter number of DPS players: ", 0, MAX_VAL);
    min_time = get_validated_input("Enter minimum dungeon time (seconds): ", 0, MAX_VAL);


    // use 15 as upper bound
    max_time = get_validated_input("Enter maximum dungeon time (seconds, t2 <= 15): ", min_time, 15);

    // Show final inputs
    std::cout << "\n--- Inputs Received ---" << std::endl;
    std::cout << "Max Instances: " << n_instances << std::endl;
    std::cout << "Tanks:         " << tank << std::endl;
    std::cout << "Healers:       " << healer << std::endl;
    std::cout << "DPS:           " << dps << std::endl;
    std::cout << "Min Time:      " << min_time << "s" << std::endl;
    std::cout << "Max Time:      " << max_time << "s" << std::endl;
}


int main() {
    inputMode();

    for (int i = 0; i < n_instances; ++i) {
        dungeons.push_back(std::make_unique<DungeonInstance>(i));
    }

    dungeon_slots.release(n_instances); // set semaphore count to n

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        for (int i = 0; i < tank; ++i) tank_queue.push(i);
        for (int i = 0; i < healer; ++i) healer_queue.push(i);
        for (int i = 0; i < dps; ++i) dps_queue.push(i);
    }

    std::cout << "\n--- Simulation Starting ---" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // start threads
    std::thread dispatcher(dispatcher_thread);
    std::thread monitor(monitor_thread);

    // wait for user input "Enter" to stop simulation
    std::cout << "\n*** Simulation is running. Press [Enter] to stop and generate the final report. ***" << std::endl;
    std::cin.get(); // This line blocks execution until you press Enter

    log_message("--- SHUTTING DOWN --- Please wait for active dungeons to finish...");
    simulation_running = false;     // Set the global flag to stop all thread loops
    party_condition.notify_all(); // Wake up any sleeping threads (especially the dispatcher)

    // join all threads
    dispatcher.join();
    monitor.join();

    // final report
    std::cout << "\n\n--- Simulation Finished ---" << std::endl;
    std::cout << "===========================" << std::endl;
    for (const auto& instance : dungeons) {
        std::cout << "  Instance " << instance->instance_id
                  << ": Served " << instance->parties_served.load() << " parties. "
                  << "Total active time: " << instance->total_time_served << "s." << std::endl;
    }
    std::cout << "===========================" << std::endl;

    return 0;
}