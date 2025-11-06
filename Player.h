//
// Created by peter on 11/7/2025.
//

#ifndef PS2_STDISCM_PLAYER_H
#define PS2_STDISCM_PLAYER_H
#include <string>

enum PlayerClass { Tank, Healer, DPS};
class Player {
    public:
        Player(int id, PlayerClass cls); // player id and assigned class

    private:
        int id;
        PlayerClass cls;
};


#endif //PS2_STDISCM_PLAYER_H