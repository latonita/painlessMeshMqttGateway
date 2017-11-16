const config = require("./config.js").config;

const eventsModule = require("events");
const ee = new eventsModule.EventEmitter();

const SerialPort = require("serialport");
const port = new SerialPort(config.serial.port, {baudRate: config.serial.baud});

const slip = require('slip');
const decoder = new slip.Decoder({});

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
  var data = new Uint8Array(str.length);
  for(var i=0,j=str.length;i<j;++i){
    data[i]=str.charCodeAt(i);
  }
  var packet = slip.encode(data)
  port.write(packet);
  port.drain();
  gwStat.mqtt.relayed++;
});


port.on('data', (data)=>{
    data = decoder.decode(data);
    if (data) {
        gwStat.mesh.received++;
        
        var cmd = String.fromCharCode.apply(null, data);
        
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
    }
});

