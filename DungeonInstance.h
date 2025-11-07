//
// Created by peter on 11/7/2025.
//

#ifndef PS2_STDISCM_DUNGEONINSTANCE_H
#define PS2_STDISCM_DUNGEONINSTANCE_H

#include <atomic>

class DungeonInstance {
public:
    DungeonInstance(int id);

    int instance_id;
    std::atomic<bool> is_active;
    std::atomic<int> parties_served;
    double total_time_served; // currently Using double for seconds (or should i change this ba?)
};


#endif //PS2_STDISCM_DUNGEONINSTANCE_H