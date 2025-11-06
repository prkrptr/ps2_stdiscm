#include <iostream>
#include <filesystem>


// global variables
//n - maximum number of concurrent instances
uint64_t n = 0;
// t - number of tank players in the queue
uint64_t tank = 0;

// h - number of healer players in the queue
uint64_t healer = 0;

// d - number of DPS players in the queue
uint64_t dps= 0;

// TODO: change proper datatype
// t1 - minimum time before an instance is finished
uint64_t min_time = 0;

// t2 - maximum time before an instance is finished
uint64_t max_time = 0;

void inputMode() {
    uint64_t number= 0;
        std::cout << "\n WELCOME TO LFG (Looking for Group) dungeon Simulator" << std::endl;
    // NO zero input (so that a party can be made?)
    std::cout << "Enter Max number of concurrent instance: ";
    std::cin >> number;
    n = number;
    std::cin.clear();

    std::cout << "Enter number of tank players: ";
    std::cin >> number;
    tank = number;
    std::cin.clear();

    std::cout << "Enter number of healer players: ";
    std::cin >> number;
    healer = number;
    std::cin.clear();

    std::cout << "Enter number of dps players: ";
    std::cin >> number;
    dps = number;
    std::cin.clear();

    std::cout << "Enter minimum time before an instance is finished: " ;
    std::cin >> number;
    min_time = number;
    std::cin.clear();

    std::cout << "Enter maximum time before an instance is finished: ";
    std::cin >> number;
    max_time = number;

    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // TODO: Validate inputs

    // validate show inputs
    std::cout << "n = " + std::to_string(n) << std::endl;
    std::cout << "tank = " + std::to_string(tank) << std::endl;
    std::cout << "healer = " + std::to_string(healer) << std::endl;
    std::cout << "dps = " + std::to_string(dps) << std::endl;
    std::cout << "min_time = " + std::to_string(min_time) << std::endl;
    std::cout << "max_time = " + std::to_string(max_time) << std::endl;
}

void displayStatusOfInstances() {
    std::cout << "OCCUPIED | VACANT" << std::endl;

}



int main() {

    inputMode();

    displayStatusOfInstances();
    return 0;
}