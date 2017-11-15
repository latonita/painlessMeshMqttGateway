const config = require("./config.js").config;

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
  mqtt.publish(config.topic.PREFIX_OUT + config.topic.GATEWAY_ID + "/" + config.topic.ONLINE, config.payload.ONLINE, {retain:true});
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

Array.prototype.diff = function(a) {
  return this.filter(function(i) {return a.indexOf(i) < 0;});
};
var onlineNodes = [];


slip.on('packet_received', (cmd) => {
  gwStat.mesh.received++;
  
  console.log("[MESH >>] " + cmd);
  try {
    var obj = JSON.parse(cmd);
    var nodes = obj["nodes"];
    if (nodes) {
      // we've got status info with list of online nodes
      var offline = onlineNodes.diff(nodes);
      var online = onlineNodes.diff(offline);
      onlineNodes = nodes;

      if (mqtt.connected === true) {
        for(var i = 0; i < offline.length; i++) {
          var topic = config.topic.PREFIX_OUT + offline[i] + "/" + config.topic.ONLINE;
          var payload = config.payload.OFFLINE;
          mqtt.publish(topic, payload, {retain: true});
          console.log('[>> MQTT] {"topic":"' + topic + '","payload":' + payload + '}');
        }
        for(var i = 0; i < online.length; i++) {
          var topic = config.topic.PREFIX_OUT + online[i] + "/" + config.topic.ONLINE;
          var payload = config.payload.ONLINE;
          mqtt.publish(topic, payload, {retain: true});
          console.log('[>> MQTT] {"topic":"' + topic + '","payload":' + payload + '}');
        }
      }

    } else // temp
    if (mqtt.connected === true) {
      // publish who's offline




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

