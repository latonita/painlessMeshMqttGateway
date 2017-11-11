#include <Arduino.h>
#include "painlessMesh.h"
#include "PacketSerial.h"

#include "..\common\mesh-config.h"

SLIPPacketSerial slipSerial;
painlessMesh mesh;

// Announce ourselves in the mesh every minute
Task meshGateAnnouncementTask(10 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["gate"] = mesh.getNodeId();

    String str;
    msg.printTo(str);
    mesh.sendBroadcast(str);
});

// Send mesh status to MQTT
Task mqttMeshStatusTask(1 * 10 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = "mesh/status";

    JsonObject& payload = msg.createNestedObject("payload");
    payload["gate"] = mesh.getNodeId();
    payload["meshSize"] = mesh.getNodeList().size();

    String str;
    msg.printTo(str);
    slipSerial.send((const uint8_t*)str.c_str(), str.length());
});

void onMeshMessageReceived( uint32_t from, String &msg ) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(msg);
    //just pass everything from mesh to mqtt gate
    if (root.containsKey("topic")) {
        String topic = String(from) + "/" + String(root["topic"].as<String>());
        root["topic"] = topic;

        String str;
        root.printTo(str);
        slipSerial.send((const uint8_t*)str.c_str(), str.length());
    }
}

void onMqttMessageReceived(const uint8_t* buffer, size_t size) {
    String str;
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buffer);
    if (root != JsonObject::invalid()) {
        uint32_t to = root["nodeId"]; //todo: better double check it is integer
        root.remove("nodeId");
        root.printTo(str);
        if (to == 0) {
            mesh.sendBroadcast(str);
        } else {
            mesh.sendSingle(to, str);
        }
    } else {
        // transmission/protocol problem. we shall not be here
    }
}

void publishOnlineStatus(uint32_t nodeId, bool online) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = String(nodeId) + "/$online";
    msg["payload"] = online ? "online" : "offline";

    String str;
    msg.printTo(str);
    slipSerial.send((const uint8_t*)str.c_str(), str.length());
}

void onNodeConnected(uint32_t nodeId){
    publishOnlineStatus(nodeId, true);
}
void onNodeDropped(uint32_t nodeId){
    publishOnlineStatus(nodeId, false);
}

void setup() {
    //Serial.begin(115200);
    //Serial.println();
    slipSerial.setPacketHandler(&onMqttMessageReceived);
    slipSerial.begin(115200);
    //mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages
    mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
    mesh.onReceive(&onMeshMessageReceived);
    mesh.onNewConnection(&onNodeConnected);
    mesh.onDroppedConnection(&onNodeDropped);

    mesh.scheduler.addTask(meshGateAnnouncementTask);
    meshGateAnnouncementTask.enable();

    mesh.scheduler.addTask(mqttMeshStatusTask);
    mqttMeshStatusTask.enable();
}

void loop() {
    mesh.update();
    slipSerial.update();
}
