#include <Arduino.h>
#include "painlessMesh.h"

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

painlessMesh _mesh;

///////////////////////////////////////////
typedef std::function<void(uint32_t from, String &msg)> meshReceivedCallback_t;
typedef std::function<void(String &topic, String &payload)> mqttReceivedCallback_t;
meshReceivedCallback_t _meshReceivedCallback;
mqttReceivedCallback_t _mqttReceivedCallback;
uint32_t _mqttGatewayId = 0;

//class painlessMeshMqtt : painlessMesh {};
// todo: refactor all, move to class
void _receivedCallback( uint32_t from, String &msg ) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(msg);
    if (root.containsKey("mesh")) {
        if (_meshReceivedCallback) {
            String s = root.get<String>("mesh");
            _meshReceivedCallback(from, s);
        }
    } else if (root.containsKey("mqttGatewayId")) {
        _mqttGatewayId = root.get<uint32_t>("mqttGatewayId");
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

bool _sendSingle(uint32_t &destId, String &msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["mesh"] = msg;
    
    String str;
    root.printTo(str);
    return _mesh.sendSingle(destId, str);
}

bool _sendBroadcast(String &msg) {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();
    root["mesh"] = msg;

    String str;
    root.printTo(str);
    return _mesh.sendBroadcast(str);
}

bool _sendMqtt(String &topic, String &payload) {
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
///////////////////////////////////////////

Task mySensorTask(1 * 5 * 1000, TASK_FOREVER, []() {
    String topic = "status/uptime";
    String payload = String(millis());
    _sendMqtt(topic, payload);
});

void onMeshMessageReceived (uint32_t from, String &msg) {
    Serial.println("MESH received");
}

void onMqttMessageReceived (String &topic, String &payload) {
    Serial.println("MQTT received");
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    _mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages

    _mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
    _mesh.onReceive(&_receivedCallback);

    _meshReceivedCallback = &onMeshMessageReceived;
    _mqttReceivedCallback = &onMqttMessageReceived;

    _mesh.scheduler.addTask(mySensorTask);
    mySensorTask.enable();
}

void loop() {
    _mesh.update();
}

