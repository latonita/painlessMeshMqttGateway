#ifndef REGULAR_NODE
#define REGULAR_NODE

#include "painlessMesh.h"
#include "../common/mesh-config.h"

painlessMesh _mesh;

typedef std::function<void (uint32_t from, String &msg)> meshReceivedCallback_t;
typedef std::function<void (String &topic, String &payload)> mqttReceivedCallback_t;
meshReceivedCallback_t _meshReceivedCallback;
mqttReceivedCallback_t _mqttReceivedCallback;
uint32_t _mqttGatewayId = 0;
uint32_t _mqttGatewayLastSeen = 0;

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
        _mqttGatewayLastSeen = millis();
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

bool meshGate_sendToGateway(JsonObject &json) {
  String str;
  json.printTo(str);

  if(millis() > _mqttGatewayLastSeen + GATEWAY_LOST_PERIOD ) {
      Serial.print("GW LOST");
      _mqttGatewayId = 0;
  }

  Serial.printf("TO GW (%d): %s\n", _mqttGatewayId, str.c_str());
  if (_mqttGatewayId == 0)
      return _mesh.sendBroadcast(str);
  else
      return _mesh.sendSingle(_mqttGatewayId, str);
}

bool meshGate_sendMqtt(String &topic, String &payloadString) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["topic"] = topic;

    JsonObject& payloadJson = jsonBuffer.parseObject(payloadString);
    if (payloadJson.success())
      root["payload"] = payloadJson;
    else
      root["payload"] = payloadString;
    meshGate_sendToGateway(root);
}

bool meshGate_sendMqtt(String &topic, JsonObject &payloadJson) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["topic"] = topic;
    root["payload"] = payloadJson;
    meshGate_sendToGateway(root);
}


ADC_MODE(ADC_VCC);  // to enable ESP.getVcc()
void getNodeSystemStatus(JsonObject& payload) {
  payload["uptime"] = millis();
  payload["chipId"] = String(_mesh.getNodeId(), HEX);
  payload["free"] = ESP.getFreeHeap();
  payload["tasks"] = _mesh.scheduler.size();
  payload["vcc"] = ESP.getVcc(); //cant get it working. getting wdt reset
}

Task mesGate_statusAnnouncementTask (5 * 1000, TASK_FOREVER, []() {
    String topic = "status"; // will be published to: mesh-out/{node-id}/status

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    getNodeSystemStatus(root);
    meshGate_sendMqtt(topic, root);
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
