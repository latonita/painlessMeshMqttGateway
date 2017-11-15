#ifndef GATE_H_
#define GATE_H_

#include "../common/mesh.h"

//void publishOnlineStatus(uint32_t nodeId, bool online);

// SLIP
SLIPPacketSerial _slipSerial;

void publishMqttOverSlip(JsonObject& json){
    String str;
    json.printTo(str);
    _slipSerial.send((const uint8_t*)str.c_str(), str.length());
}

// MQTT
void mqttPublishOnlineStatus(uint32_t nodeId, bool online) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = String(nodeId) + "/$online";
    msg["payload"] = online ? "online" : "offline";
    msg["qos"] = 1;
    publishMqttOverSlip(msg);
}

void onMqttMessageReceived(const uint8_t* buffer, size_t size) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(buffer);
    if (root != JsonObject::invalid()) {
        uint32_t to = root["nodeId"]; //todo: better double check it is integer
        root.remove("nodeId");
        String str;
        root.printTo(str);
        if (to == 0) {
            _mesh.sendBroadcast(str);
        } else {
            _mesh.sendSingle(to, str);
        }
    } else {
        // transmission/protocol problem. we shall not be here
    }
}

// MESH comms
void onMeshMessageReceived( uint32_t from, String &msg ) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(msg);
    //just pass everything from mesh to mqtt gate
    if (json.containsKey("topic")) {
        String topic = String(from) + "/" + String(json["topic"].as<String>());
        json["topic"] = topic;
        publishMqttOverSlip(json);
    }
}

void onMeshNodeConnected(uint32_t nodeId) {
    mqttPublishOnlineStatus(nodeId, true);
}
void onMeshNodeDropped(uint32_t nodeId) {
    mqttPublishOnlineStatus(nodeId, false);
}

// TASKS
Task mqttNodeStatusTask (5 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["topic"] = String(_mesh.getNodeId()) + "/status";
    JsonObject& payload = root.createNestedObject("payload");
    getNodeSystemStatus(payload);
    publishMqttOverSlip(root);
});

// Announce ourselves in the mesh every minute
Task meshGateAnnouncementTask(GATEWAY_ANNOUNCEMENT_PERIOD, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["gate"] = _mesh.getNodeId();

    String str;
    msg.printTo(str);
    _mesh.sendBroadcast(str);
});

// Send mesh status to MQTT
Task mqttMeshStatusTask(1 * 10 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = "mesh/status";
    JsonObject& payload = msg.createNestedObject("payload");
    msg["payload"]["gate"] = _mesh.getNodeId();
    msg["payload"]["meshSize"] = _mesh.getNodeList().size() + 1;
    publishMqttOverSlip(msg);

    jsonBuffer.clear(); // mag and payload object are gone

    JsonObject& msg2 = jsonBuffer.createObject();
    JsonArray& list = msg2.createNestedArray("nodes");
    auto nl = _mesh.getNodeList();
    for (auto const& n : _mesh.getNodeList()) {
        list.add(n);
    }
    list.add(_mesh.getNodeId());
    publishMqttOverSlip(msg2);
});

void meshInit() {
    //    _mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages
        _mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
        _mesh.onReceive(&onMeshMessageReceived);
        _mesh.onNewConnection(&onMeshNodeConnected);
        _mesh.onDroppedConnection(&onMeshNodeDropped);

        _mesh.scheduler.addTask(mqttNodeStatusTask);
        mqttNodeStatusTask.enable();
    
        _mesh.scheduler.addTask(mqttMeshStatusTask);
        mqttMeshStatusTask.enable();
        
        _mesh.scheduler.addTask(meshGateAnnouncementTask);
        meshGateAnnouncementTask.enable();
    }
    

#endif
