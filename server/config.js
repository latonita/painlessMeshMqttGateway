var config = {};

config.serial = {};
config.serial.port = process.env.MESH_SERIAL_PORT || "COM6"; // "/dev/ttyUSB3"
config.serial.baud = process.env.MESH_SERIAL_BAUD || 115200;

config.mqtt = {};
config.mqtt.server = process.env.MESH_MQTT_SERVER_URL || "mqtt://tank:1883";
//config.mqtt.password = ""; //todo: implement mqtt password, its not used at the moment

config.topic = {};
config.topic.PREFIX_IN = "mesh-in/";
config.topic.PREFIX_OUT = "mesh-out/";
config.topic.GATEWAY_ID = "gateway";
config.topic.ONLINE = "$online";
config.topic.STATUS = "status";

config.payload = {};
config.payload.ONLINE = "online";
config.payload.OFFLINE = "offline";

exports.config = config;
