#ifdef BUILD_GATEWAY
#include <Arduino.h>
#include "painlessMesh.h"
#include "PacketSerial.h"

#include "gate.h"
#include "..\common\mesh-config.h"

SLIPPacketSerial slipSerial;
//painlessMesh mesh;
void publishOnlineStatus(uint32_t nodeId, bool online);



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
    msg["payload"]["gate"] = mesh.getNodeId();
    msg["payload"]["meshSize"] = mesh.getNodeList().size() + 1;
    // JsonObject& payload = msg.createNestedObject("payload");
    // payload["gate"] = mesh.getNodeId();
    // payload["meshSize"] = mesh.getNodeList().size() + 1;

    String str;
    msg.printTo(str);
    slipSerial.send((const uint8_t*)str.c_str(), str.length());
});


//ADC_MODE(ADC_VCC);  // to enable ESP.getVcc()
void getNodeSystemStatus(JsonObject& payload) {
  payload["uptime"] = millis();
  payload["chipId"] = String(mesh.getNodeId(), HEX);
  payload["free"] = ESP.getFreeHeap();
  payload["tasks"] = mesh.scheduler.size();
  //payload["vcc"] = ESP.getVcc(); // nothing interesting here..
}

Task mqttNodeStatusTask (5 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["topic"] = String(mesh.getNodeId()) + "/status";

    JsonObject& payload = root.createNestedObject("payload");

    getNodeSystemStatus(payload);


    // root["payload"]["uptime"] = millis();
    // root["payload"]["chipId"] = String(mesh.getNodeId(), HEX);
    // root["payload"]["free"] = ESP.getFreeHeap();
    // root["payload"]["tasks"] = mesh.scheduler.size();

    // JsonObject& payload = root.createNestedObject("payload");
    // payload["uptime"] = millis();
    // payload["chipId"] = String(ESP.getChipId(), HEX);
    // payload["free"] = ESP.getFreeHeap();
    // payload["tasks"] = mesh.scheduler.size();

    String str;
    root.printTo(str);
    slipSerial.send((const uint8_t*)str.c_str(), str.length());
    publishOnlineStatus(mesh.getNodeId(),true);

});

void publishOnlineStatus(uint32_t nodeId, bool online) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = String(nodeId) + "/$online";
    msg["payload"] = online ? "online" : "offline";

    String str;
    msg.printTo(str);
    slipSerial.send((const uint8_t*)str.c_str(), str.length());
}


void onMeshMessageReceived( uint32_t from, String &msg ) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(msg);
    //just pass everything from mesh to mqtt gate
    if (json.containsKey("topic")) {
        String topic = String(from) + "/" + String(json["topic"].as<String>());
        json["topic"] = topic;
        String str;
        json.printTo(str);
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
//    mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages
    mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
    mesh.onReceive(&onMeshMessageReceived);
    mesh.onNewConnection(&onNodeConnected);
    mesh.onDroppedConnection(&onNodeDropped);

    mesh.scheduler.addTask(mqttMeshStatusTask);
    mqttMeshStatusTask.enable();

    mesh.scheduler.addTask(mqttNodeStatusTask);
    mqttNodeStatusTask.enable();

    mesh.scheduler.addTask(meshGateAnnouncementTask);
    meshGateAnnouncementTask.enable();

}

void loop() {
    mesh.update();
    slipSerial.update();
}
#endif //BUILD_GATEWAY
