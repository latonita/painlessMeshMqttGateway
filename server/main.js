
const eventsModule = require("events");
const ee = new eventsModule.EventEmitter();

const slipModule = require("serialport-slip");
const slip = new slipModule("COM6", {
  baudRate: 115200
});
var slipCommandBuffer = "";


const MQTT_IN_PREFIX = "mesh-in/";
const MQTT_OUT_PREFIX = "mesh-out/";

const mqttModule = require("mqtt");
const mqtt = mqttModule.connect('mqtt://tank.local:1883', {
  will: {
    topic: MQTT_OUT_PREFIX + "gateway/$online",
    payload: "offline"
  }
});

var gwStat = {
  mesh : {
    received : 0,
    relayed : 0,
    dropped : 0 
  },
  mqtt : {
    received : 0,
    relayed : 0,
    dropped : 0
  }
}

function gwAnnounce() {
  mqtt.publish(MQTT_OUT_PREFIX + "gateway/$online", "online");
  mqtt.publish(MQTT_OUT_PREFIX + "gateway/status", JSON.stringify(gwStat));
}

mqtt.on('connect', () => {
  gwAnnounce();
  mqtt.subscribe(MQTT_IN_PREFIX + "#");
});

mqtt.on('message', (topic, message) => {
  var payload = message.toString();
  console.log("[MQTT >>] topic = " + topic + ", payload = " + payload);

  gwStat.mqtt.received++;
  
  if (topic.startsWith(MQTT_IN_PREFIX)) {
    var topic = topic.slice(MQTT_IN_PREFIX.length);
    var recepient = topic.slice(0, topic.indexOf("/"));

    switch(recepient) {
      case "gateway": {
        var sub = topic.slice(topic.indexOf("/") + 1);        
        console.log("[OK] Gateway control. sub = " + sub);
        if (sub === "status") {
          gwAnnounce();          
        }
        break;
      }
      
      default: 
        if (/^(\-|\+)?([0-9]+|Infinity)$/.test(recepient)) {
          //var nodeId = Number(recepient);
//          console.log("[OK] Topic = " + topic + ", payload = " + payload);
          ee.emit('slip_send', topic, payload);
        } else {
          console.log("[ERROR] Impossible nodeId");
          gwStat.mqtt.dropped++;
        }
      }
    }
  }
);

ee.on("slip_send" , (topic, payload) => {
  var obj = {
    topic: topic,
    payload: payload
  }
  var str = JSON.stringify(obj);
  console.log("[>> MESH] " + str);
  slip.sendMessage(Buffer.from(str,"utf8"));
  gwStat.mqtt.relayed++;
});

ee.on("slip_received", (cmd) => {
  gwStat.mesh.received++;
  console.log("[MESH >>] " + cmd);
  try {
    var obj = JSON.parse(cmd);
    if (mqtt.connected === true) {
      mqtt.publish(MQTT_OUT_PREFIX + obj["topic"], obj["payload"].toString());
      console.log("[>> MQTT] " + MQTT_OUT_PREFIX + obj["topic"]);
      gwStat.mesh.relayed++;
    } else {
      gwStat.mesh.dropped++;
      console.log("[ERROR] No MQTT connection");
    }
  } catch (ex) {
      console.log("[ERROR] " + ex.name + ", " + ex.message);
      gwStat.mesh.dropped++;
  }
});

slip.on('message',  (message) => {
  var commands = new Array();
  var str = message.toString();
  slipCommandBuffer = slipCommandBuffer.concat(str);

  var firstEnd = 0;
  firstEnd = slipCommandBuffer.indexOf(slip.endByte_);
  if (firstEnd == -1) {
    //dirty hack - sometimes END is ommitted by SLIP library for some unknown reason
    //lets check if it is normal json then its command, else - continue collecting parts
    try {
      var js = JSON.parse(slipCommandBuffer);
      commands.push(slipCommandBuffer);
      slipCommandBuffer ="";
    } catch (e) {
      debugger;
    }
  }
  while (firstEnd >= 0) {
    var cmd = slipCommandBuffer.substr(0, firstEnd);
    if (cmd.length > 0) {
      commands.push(cmd);
    }
    slipCommandBuffer = slipCommandBuffer.slice(firstEnd + 1);
    firstEnd = slipCommandBuffer.indexOf(slip.endByte_);
  }

  for (cmd of commands) {
    ee.emit('slip_received', cmd);
  }
})

slip.on('error', (err) => {
  console.log("[FATAL] " + err.toString());
});

slip.on('close', () => {
  console.log("[FATAL] Disconnect. Port closed.");
});