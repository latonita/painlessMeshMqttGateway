#ifdef BUILD_GATEWAY
#include <Arduino.h>
#include "PacketSerial.h"

#include "gate.h"

void setup() {
    //Serial.begin(115200);
    //Serial.println();
    _slipSerial.setPacketHandler(&onMqttMessageReceived);
    _slipSerial.begin(115200);
    meshInit();
}

void loop() {
    _mesh.update();
    _slipSerial.update();
}
#endif //BUILD_GATEWAY
