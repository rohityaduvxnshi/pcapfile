#ifndef UDPPACKETPARSER_H
#define UDPPACKETPARSER_H

#include "AppTypes.h"

class UdpPacketParser
{
public:
    static ParsedUdpPacket parsePacket(const RawPacket& packet);
};

#endif
