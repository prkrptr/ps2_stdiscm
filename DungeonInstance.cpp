//
// Created by peter on 11/7/2025.
//

#include "DungeonInstance.h"


DungeonInstance::DungeonInstance(int id) {
    instance_id = id;
    is_active = false; // Initially empty
    parties_served = 0;
    total_time_served = 0.0;
}