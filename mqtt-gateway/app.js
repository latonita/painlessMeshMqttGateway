const config = require("./config.js").config;

const eventsModule = require("events");
const ee = new eventsModule.EventEmitter();

const serialPort = require("serialport");
const port = new serialPort(config.serial.port, {baudRate: config.serial.baud});

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
  console.log("[MQTT >>] topic = " + topic + ", payload = " + payload);

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
  var obj = {
    topic: topic,
    nodeId: nodeId,
    payload: payload
  }
  var str = JSON.stringify(obj);
  console.log("[>> MESH] " + str);
  slipSendAndDrain(str);
  gwStat.mqtt.relayed++;
});

ee.on("slip_received", (packet) => {
  var cmd = slipUnescape(packet).toString();
  gwStat.mesh.received++;
  
  console.log("[MESH >>] " + cmd);
  try {
    var obj = JSON.parse(cmd);
    if (mqtt.connected === true) {
      mqtt.publish(config.topic.PREFIX_OUT + obj["topic"], JSON.stringify(obj["payload"]));
      console.log("[>> MQTT] " + config.topic.PREFIX_OUT + obj["topic"]);
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


var dataBuf = Buffer.alloc(0);
const SLIP_END = 192;
const SLIP_ESC = 219;
const SLIP_ESC_END = 220;
const SLIP_ESC_ESC = 221;

function slipUnescape(buf){
  var res = [];
  var esc = false;
  for (var i = 0; i < buf.length; i++) {
    var cur = buf[i];
    if (esc) {
      esc = false;
      if (cur === SLIP_ESC_END) {
        cur = SLIP_END;
      } else if (cur === SLIP_ESC_ESC) {
        cur = SLIP_ESC;
      } else {
        // we shall not be here. protocol error.
        // do nothing for now...
      }
    } else if (cur === SLIP_ESC) {
      esc = true;
    } else {
      esc = false;
    }
    if (!esc)
      res.push(cur);
  }
  return new Buffer(res);
}

function escapeAndWrite(buf){
  var res = [];
  for (var i = 0; i < buf.length; i++) {
    var cur = buf[i];
    if (cur === SLIP_END) {
      res.push([SLIP_ESC,SLIP_ESC_END]);
    } else if (cur === SLIP_ESC) {
      res.push([SLIP_ESC,SLIP_ESC_ESC]);
    } else {
      res.push(cur);
    }
  }
  port.write(new Buffer(res));
}

const SLIP_END_BUF = new Buffer(SLIP_END);

function slipSendAndDrain(str){
  port.write(SLIP_END_BUF);
  escapeAndWrite(str);
  port.write(SLIP_END_BUF);
  port.drain();
}

port.on('data', (data)=> {
  if (data.length == 0)
    return;

  dataBuf = Buffer.concat([dataBuf,data]);
  var offset = 0;
  var packets = new Array();
  var end = 0;
  end = dataBuf.indexOf(SLIP_END);
  while (end >= 0) {
    if (end - offset > 0) {
      var p = dataBuf.slice(offset, end);
      packets.push(p);
    }

    offset = end + 1;

    end = dataBuf.indexOf(SLIP_END, offset);
  }
  dataBuf = dataBuf.slice(offset);

  for (p of packets) {
    ee.emit('slip_received', p);
  }
});


port.on('error', (err) => {
  console.log("[FATAL] " + err.toString());
});

port.on('close', () => {
  console.log("[FATAL] Disconnect. Port closed.");
});