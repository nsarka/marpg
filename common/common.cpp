#include "common/common.hpp"

#include "settings.hpp"
#include <algorithm>

namespace common {

void writeInputCmd(sf::Packet& packet, const common::PlayerId& id, const common::InputCommand& cmd) {
    packet << std::string(MSG_STATE);
    packet << id << cmd.sequence << cmd.move.x << cmd.move.y << cmd.sprint << cmd.lightHeld
           << cmd.lightPressed << cmd.lightReleased << cmd.heavyHeld << cmd.heavyPressed << cmd.heavyReleased
           << cmd.aim.x << cmd.aim.y << cmd.hasCursor << cmd.cursor.x << cmd.cursor.y << cmd.movementFacing;
}

bool readInputCmd(sf::Packet& packet, common::PlayerId& id, common::InputCommand& cmd) {
    float moveX = 0.f;
    float moveY = 0.f;

    if (!(packet >> id >> cmd.sequence >> moveX >> moveY >> cmd.sprint >> cmd.lightHeld >> cmd.lightPressed >>
          cmd.lightReleased >> cmd.heavyHeld >> cmd.heavyPressed >> cmd.heavyReleased >> cmd.aim.x >>
          cmd.aim.y)) {
        return false;
    }

    if (!packet.endOfPacket() && !(packet >> cmd.hasCursor >> cmd.cursor.x >> cmd.cursor.y))
        return false;
    if (!packet.endOfPacket() && !(packet >> cmd.movementFacing))
        return false;
    cmd.move.x = moveX;
    cmd.move.y = moveY;

    return true;
}

} // namespace common
