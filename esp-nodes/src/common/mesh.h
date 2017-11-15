#ifndef MESH_H_
#define MESH_H_

#include "mesh-config.h"
#include "painlessMesh.h"
painlessMesh _mesh;

void getNodeSystemStatus(JsonObject& payload) {
    payload["uptime"] = millis();
    payload["free"] = ESP.getFreeHeap();
    payload["chipId"] = String(_mesh.getNodeId(), HEX);
    payload["tasks"] = _mesh.scheduler.size();
}

#endif

