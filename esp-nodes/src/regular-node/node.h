#ifndef REGULAR_NODE
#define REGULAR_NODE

#include "painlessMesh.h"

painlessMesh _mesh;

typedef std::function<void (uint32_t from, String &msg)> meshReceivedCallback_t;
typedef std::function<void (String &topic, String &payload)> mqttReceivedCallback_t;
meshReceivedCallback_t _meshReceivedCallback;
mqttReceivedCallback_t _mqttReceivedCallback;
uint32_t _mqttGatewayId = 0;

//class painlessMeshMqtt : painlessMesh {};
// todo: refactor all, move to class
void meshGate_onReceiveMeshMessage(meshReceivedCallback_t cb){
    _meshReceivedCallback = cb;
}
void meshGate_onReceiveMqttMessage(mqttReceivedCallback_t cb){
    _mqttReceivedCallback = cb;
}

void meshGate_receivedCallback( uint32_t from, String &msg ) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(msg);
    if (root.containsKey("m")) {
        if (_meshReceivedCallback) {
            String s = root.get<String>("m");
            _meshReceivedCallback(from, s);
        }
    } else if (root.containsKey("gate")) {
        _mqttGatewayId = root.get<uint32_t>("gate");
    } else if (root.containsKey("topic")) {
        if (_mqttReceivedCallback) {
            String t = root.get<String>("topic");
            String p = root.get<String>("payload");
            _mqttReceivedCallback(t,p);
        }
    } else {
        // debuMsg (ERROR, unexpected json)
    }
}

bool meshGate_sendSingle(uint32_t &destId, String &msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["m"] = msg;

    String str;
    root.printTo(str);
    return _mesh.sendSingle(destId, str);
}

bool meshGate_sendBroadcast(String &msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["m"] = msg;

    String str;
    root.printTo(str);
    return _mesh.sendBroadcast(str);
}

bool meshGate_sendMqtt(String &topic, String &payload) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["topic"] = topic;
    root["payload"] = payload;

    String str;
    root.printTo(str);
    if (_mqttGatewayId == 0)
        return _mesh.sendBroadcast(str);
    else
        return _mesh.sendSingle(_mqttGatewayId, str);
}

Task mesGate_statusAnnouncementTask (5 * 1000, TASK_FOREVER, []() {
    String topic = "status/uptime"; // will be published to: mesh-out/{node-id}/status/uptime
    String payload = String(millis());

//    payload["mem"] = ESP.getFreeHeap();
//    payload["tasks"] = mesh.scheduler.size();

    meshGate_sendMqtt(topic, payload);
});

void meshGate_init(unsigned long announceIntervalSeconds = 0) {
    _mesh.onReceive(&meshGate_receivedCallback);
    if (announceIntervalSeconds > 0) {
        _mesh.scheduler.addTask(mesGate_statusAnnouncementTask);
        mesGate_statusAnnouncementTask.setInterval(announceIntervalSeconds * 1000);
        mesGate_statusAnnouncementTask.enable();
    }
}

#endif
