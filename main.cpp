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

void run_dungeon(int instance_id) {
    DungeonInstance& dungeon = *dungeons[instance_id];

    dungeon.is_active = true;
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout <<"[Party] A party has entered dungeon " << dungeon.instance_id << "." << std::endl;

    }


    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min_time, max_time > min_time ? max_time : min_time);
    int run_time_sec = distrib(gen);

    std::this_thread::sleep_for(std::chrono::seconds(run_time_sec));

    //update stats and announce exit
    dungeon.is_active = false;
    dungeon.parties_served++;
    dungeon.total_time_served += run_time_sec;

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[Party] Party in instance " << dungeon.instance_id << " finished in " << run_time_sec << "s. Slot is now free." << std::endl;
    }

    dungeon_slots.release(); // release the semaphore slot

    party_condition.notify_one(); // notifies the dispatcher in case it's waiting
}


// dispatcher logic
void dispatcher_thread() {
    int total_parties_possible = std::min({tank, healer, dps/3});
    int parties_formed = 0;

    while (parties_formed < total_parties_possible) {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // wait until there's enough players for a party
        party_condition.wait(lock,[] {
            return tank_queue.size() >= 1 && healer_queue.size() >= 1 && dps_queue.size() >= 3;
        });

        // if condition is met -> form a party
        tank_queue.pop();
        healer_queue.pop();
        dps_queue.pop();
        dps_queue.pop();
        dps_queue.pop();


        lock.unlock();

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Dispatcher] Party formed! Waiting for an available dungeon..." << std::endl;
        }

        // wait until a dungeon is free (blocks if all are busy)
        dungeon_slots.acquire();

        // find a vacant dungeon instance to put the party into
        for (auto& dungeon_ptr : dungeons) {
            if (!dungeon_ptr->is_active) {
                std::thread(run_dungeon, dungeon_ptr->instance_id).detach();
                break;
            }
        }
        parties_formed++;
    }

    // all possible parties have been formed already
    // wait for the last running parties to finish before stoping the monitor
    // reacquire all semaphore slots to show that all dungeons are empty
    for (int i = 0; i < n_instances; i++) {
        dungeon_slots.acquire();
    }

    simulation_running =false;

}


// show status of instances and queues
void monitor_thread() {
    while (simulation_running) {
        // std::cout << "\033[2J\033[1;1H"; //clear screen

        std::cout << "--- LFG Dungeon Simulator ---" << std::endl;
        std::cout << "=============================" << std::endl;
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
        std::cout << "=============================" << std::endl;
        std::cout << "(Status update every 2 seconds)" << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
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

            // Check if the entire string was consumed by stoi.
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


/**
 * @brief Handles all user input using a robust validation helper function.
 */
void inputMode() {
    const int MAX_VAL = std::numeric_limits<int>::max();

    std::cout << "\nWELCOME TO LFG (Looking for Group) dungeon Simulator" << std::endl;

    // Get validated inputs using the helper function
    n_instances = get_validated_input("Enter Max number of concurrent instances: ", 1, MAX_VAL);
    tank = get_validated_input("Enter number of tank players: ", 0, MAX_VAL);
    healer = get_validated_input("Enter number of healer players: ", 0, MAX_VAL);
    dps = get_validated_input("Enter number of DPS players: ", 0, MAX_VAL);
    min_time = get_validated_input("Enter minimum dungeon time (seconds): ", 0, MAX_VAL);

    // For max_time, the minimum valid value is the min_time we just got.
    // The spec says t2 <= 15, so we use that as the upper bound.
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

    dungeon_slots.release(n_instances); // Set semaphore count to n

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

    // wait for threads to complete
    dispatcher.join();
    monitor.join();

    // final summary / report
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