#include <Arduino.h>
#include "painlessMesh.h"
#include "PacketSerial.h"

SLIPPacketSerial slip;

#define   MESH_SSID       "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

painlessMesh mesh;

void onMqttPacketReceived (const uint8_t* buffer, size_t size);
void onMeshMessageReceived (uint32_t from, String &msg);

// Announce ourselves in the mesh every minute
Task meshGateAnnouncementTask(1 * 10 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = "mqttGateway";
    msg["nodeId"] = mesh.getNodeId();

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
    payload["mem"] = ESP.getFreeHeap();
    payload["tasks"] = mesh.scheduler.size();
    payload["meshSize"] = mesh.getNodeList().size();

    String str;
    msg.printTo(str);
    slip.send((const uint8_t*)str.c_str(), str.length());
});

// Send mesh status to MQTT
Task mqttSensorTask(1 * 5 * 1000, TASK_FOREVER, []() {
    DynamicJsonBuffer jsonBuffer;
    JsonObject& msg = jsonBuffer.createObject();
    msg["topic"] = String(mesh.getNodeId()) + String("/status");
    msg["payload"] = millis();

    String str;
    msg.printTo(str);
    slip.send((const uint8_t*)str.c_str(), str.length());
});

void setup() {
    //Serial.begin(115200);
    //Serial.println();
    slip.setPacketHandler(&onMqttPacketReceived);
    slip.begin(115200);
    //mesh.setDebugMsgTypes( ERROR | CONNECTION | S_TIME );  // set before init() so that you can see startup messages

    mesh.init( MESH_SSID, MESH_PASSWORD, MESH_PORT, STA_AP, AUTH_WPA2_PSK, 6 );
    mesh.onReceive(&onMeshMessageReceived);

    mesh.onNewConnection([](size_t nodeId) {
//        Serial.printf("New Connection %u\n", nodeId);
    });

    mesh.onDroppedConnection([](size_t nodeId) {
//        Serial.printf("Dropped Connection %u\n", nodeId);
    });

    // Add the task to the mesh scheduler
    mesh.scheduler.addTask(meshGateAnnouncementTask);
    meshGateAnnouncementTask.enable();

    mesh.scheduler.addTask(mqttMeshStatusTask);
    mqttMeshStatusTask.enable();

    mesh.scheduler.addTask(mqttSensorTask);
    mqttSensorTask.enable();
}

void loop() {
    mesh.update();
    slip.update();
}

void onMeshMessageReceived( uint32_t from, String &msg ) {
    //pass everything from mesh to mqtt gate
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(msg);

    if (root.containsKey("topic")) {
        String topic = String(from) + "/" + String(root["topic"].as<String>());
        root["topic"] = topic;

        String retrans;
        root.printTo(retrans);
        slip.send((const uint8_t*)retrans.c_str(), retrans.length());
    }
}

void onMqttPacketReceived(const uint8_t* buffer, size_t size) {
  String str;
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(buffer);
  
  //// TEST BEGINS
  JsonObject& out = jsonBuffer.createObject();
  if (root == JsonObject::invalid()) {
    out["topic"] = String(mesh.getNodeId()) + "/error";
    out["payload"] = buffer;
    out.printTo(str);
  } else {
    out["topic"] = String(mesh.getNodeId()) + "/loopback";
    out["payload"] = root;
    out.printTo(str);
  }
  // as a test just send it back to mqtt
  slip.send((const uint8_t*)str.c_str(), str.length());
  //// TEST ENDS
  // now goes real stuff
  if (root != JsonObject::invalid()) {
      size_t to = root["nodeId"]; //todo: better double check it is integer
      root.remove("nodeId");
      root.printTo(str);
      if (to == 0) {
          mesh.sendBroadcast(str);    
      } else {
          mesh.sendSingle(to, str);
      }
  }
}
