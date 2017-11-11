#include <Arduino.h>
#include "painlessMesh.h"

#include "node.h"
#include "..\common\mesh-config.h"

Task mySensorTask(1 * 5 * 1000, TASK_FOREVER, []() {
    String topic = "status/uptime"; // will be published to: mesh-out/{node-id}/status/uptime
    String payload = String(millis());
    meshGate_sendMqtt(topic, payload);
});

void onMeshMsg(uint32_t from, String &msg) {
    Serial.printf("Mesh message: Received from %u msg=%s\n", from, msg.c_str());
}

void onMqttMsg(String &topic, String &payload) {
    Serial.printf("Mqtt message: Topic=%s, Payload=%s\n", topic.c_str(), payload.c_str());
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    _mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages
    _mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
    meshGate_init();
    meshGate_onReceiveMeshMessage(&onMeshMsg);
    meshGate_onReceiveMqttMessage(&onMqttMsg);
    _mesh.scheduler.addTask(mySensorTask);
    mySensorTask.enable();
}

void loop() {
    _mesh.update();
}
