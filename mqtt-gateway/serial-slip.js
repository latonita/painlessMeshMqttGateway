const SerialPort = require('serialport');

const MARKER = {
  END : 192,
  ESC : 219,
  ESC_END : 220,
  ESC_ESC : 221
};


class SerialSlip extends SerialPort {
  
  constructor (serialPort, serialOptions) {
    super(serialPort, serialOptions);
    
    this.residueBuffer = Buffer.alloc(0);
    this.END_BUF = new Buffer(MARKER.END);
    
    this.on('error', (err) => {
      console.log("[FATAL] " + err.toString());
      process.exit(1);
    });
    
    this.on('close', () => {
      console.log("[FATAL] Disconnect. Port closed.");
      process.exit(1);
    });  

    this.on('data', (data) => { 
      this.onDataIn(data);
    });
   
  }

  unescape(buf) {
    var res = [];
    var esc = false;
    for (var i = 0; i < buf.length; i++) {
      var cur = buf[i];
      if (esc) {
        esc = false;
        if (cur === MARKER.ESC_END) {
          cur = MARKER.END;
        } else if (cur === MARKER.ESC_ESC) {
          cur = MARKER.ESC;
        } else {
          // we shall not be here. protocol error.
          // do nothing for now...
        }
      } else if (cur === MARKER.ESC) {
        esc = true;
      } else {
        esc = false;
      }
      if (!esc)
        res.push(cur);
    }
    return new Buffer(res);
  }
  
  escape(buf) {
    var res = [];
    for (var i = 0; i < buf.length; i++) {
      var cur = buf[i];
      if (cur === MARKER.END) {
        res.push([MARKER.ESC,MARKER.ESC_END]);
      } else if (cur === MARKER.ESC) {
        res.push([MARKER.ESC,MARKER.ESC_ESC]);
      } else {
        res.push(cur);
      }
    }
    return new Buffer(res);
  }
    
  sendPacket(str) {
    this.write(this.END_BUF);
    this.write(escape(str));
    this.write(this.END_BUF);
    
  }

  sendPacketAndDrain(str) {
    this.sendPacket(str);
    this.drain();
  }
    
  onDataIn(data) {
    if (data.length == 0)
      return;
  
    this.residueBuffer = Buffer.concat([this.residueBuffer,data]);

    var offset = 0;
    var packets = new Array();
    var end = 0;
    end = this.residueBuffer.indexOf(MARKER.END);
    while (end >= 0) {
      if (end - offset > 0) {
        var p = this.residueBuffer.slice(offset, end);
        packets.push(p);
      }
      offset = end + 1;
  
      end = this.residueBuffer.indexOf(MARKER.END, offset);
    }
    this.residueBuffer = this.residueBuffer.slice(offset);
  
    for (p of packets) {
      let data = unescape(p).toString();
      this.emit('packet_received', data);
    }
  }
}
 
module.exports = SerialSlip;
