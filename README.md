# MQTT gateway for painlessMesh network

![](overview.png)

Gateway node in mesh network connects to mqtt gateway via serial connection.

## Message relay
### Mesh ==> MQTT
All messages inside mesh network which sent to esp-mesh-gateway will be relayed to MQTT broker.
Mesh message
```
{ "topic": "sensorName", "payload":"........." }
```
relayed to MQTT broker as 
```
{ "topic": "mesh-out/XXXX/sensorName", "payload":"........." }
```
where XXXX is mesh.nodeId().

### MQTT ==> Mesh
MQTT messages sent to "mesh_in/XXXX/blahblah" topics are being relayed to mesh.

```
{ "topic": "mesh-in/XXXX/sensorName", "payload":"........." }
```
relayed to mesh node XXXX as
```
{ "topic": "blahblah", "payload":"........." }
```
MQTT messages sent to ``"mesh_in/0/bla-blah"`` topics are being relayed to all mesh nodes as a `broadcast message`.



## esp-mesh-node 
regular nodes in mesh
### build
## esp-mesh-gateway
gateway node in mesh
### build
## node-painlessmesh-mqtt-gate
gateway node on server
### build
1. -- at first install node.js and npm (i tried on node 8.x, npm 5.x)
2. cd node-painlessmesh-mqtt-gate
3. edit config.js or export env variables (look inside file)
3. npm install
4. npm start


