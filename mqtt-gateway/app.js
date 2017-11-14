const config = require("./config.js").config;
const pjson = require("./package.json");

const eventsModule = require("events");
const ee = new eventsModule.EventEmitter();

const SerialSlip = require("./serial-slip.js");
const slip = new SerialSlip(config.serial.port, {baudRate: config.serial.baud});

const mqttModule = require("mqtt");
const mqtt = mqttModule.connect(config.mqtt.server, {
  will: {
    topic: config.topic.PREFIX_OUT + config.topic.GATEWAY_ID + "/" + config.topic.ONLINE,
    payload: config.payload.OFFLINE
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
  mqtt.publish(config.topic.PREFIX_OUT + config.topic.GATEWAY_ID + "/" + config.topic.ONLINE, config.payload.ONLINE);
  mqtt.publish(config.topic.PREFIX_OUT + config.topic.GATEWAY_ID + "/" + config.topic.STATUS, JSON.stringify(gwStat));
}

mqtt.on('connect', () => {
  gwAnnounce();
  mqtt.subscribe(config.topic.PREFIX_IN + "#");
});

mqtt.on('message', (topic, message) => {
  var payload = message.toString();
  console.log('[MQTT >>] {"topic":"' + topic + '","payload":"' + payload + '"}');

  gwStat.mqtt.received++;
  
  if (topic.startsWith(config.topic.PREFIX_IN)) {
    var topic = topic.slice(config.topic.PREFIX_IN.length);
    var recepient = topic.slice(0, topic.indexOf("/"));

    switch(recepient) {
      case "gateway": {
        var sub = topic.slice(topic.indexOf("/") + 1);
        console.log("[OK] Gateway control. sub = " + sub);
        if (sub === config.topic.STATUS) {
          gwAnnounce();          
        }
        break;
      }
      
      default: 
        if (/^(\-|\+)?([0-9]+|Infinity)$/.test(recepient)) {
          var nodeId = Number(recepient);
          var sub = topic.slice(topic.indexOf("/") + 1);
          ee.emit('slip_send', sub, nodeId, payload);
        } else {
          console.log("[ERROR] Impossible nodeId");
          gwStat.mqtt.dropped++;
        }
      }
    }
  }
);

ee.on("slip_send" , (topic, nodeId, payload) => {
  var _payload;
  try {
    _payload = JSON.parse(payload);
  } catch(e) {
    _payload = payload;
  }

  var obj = {
    topic: topic,
    nodeId: nodeId,
    payload: _payload
  }
  var str = JSON.stringify(obj);
  console.log("[>> MESH] " + str);
  slip.sendPacketAndDrain(str);
  gwStat.mqtt.relayed++;
});

slip.on('packet_received', (cmd) => {
  gwStat.mesh.received++;
  
  console.log("[MESH >>] " + cmd);
  try {
    var obj = JSON.parse(cmd);
    if (mqtt.connected === true) {
      var topic = config.topic.PREFIX_OUT + obj["topic"];
      var payload = JSON.stringify(obj["payload"]);
      mqtt.publish(topic, payload);
      console.log('[>> MQTT] {"topic":"' + topic + '","payload":' + payload + '}');
      
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

function printVersion() {
    console.log("===============================================================");
    console.log(":       MQTT Gateway for painlessMesh IoT mesh network        :");
    console.log(": (c) Anton Viktorov, latonita@yandex.ru, github.com/latonita :");
    console.log("===============================================================");
    console.log("Serial port: " + config.serial.port + " @ " + config.serial.baud);
    console.log("MQTT broker: " + config.mqtt.server);
    console.log("Topic Out: " + config.topic.PREFIX_OUT);
    console.log("Topic In: " + config.topic.PREFIX_IN);
    console.log();
}

printVersion();
