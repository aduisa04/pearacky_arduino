// server.js

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const WebSocket = require('ws');

// 1) Open the serial port (adjust path to your Arduino's COM port)
const port = new SerialPort({ path: 'COM6', baudRate: 9600 }, err => {
  if (err) {
    console.error('Error opening serial port:', err.message);
    process.exit(1);
  }
  console.log('Serial port open on COM6 @ 9600 baud');
});

// 2) Create a parser to read lines (\n-delimited) from the Arduino
const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));

// 3) Start the WebSocket server
const WS_PORT = 8080;
const wss = new WebSocket.Server({ port: WS_PORT }, () => {
  console.log(`WebSocket server running on ws://localhost:${WS_PORT}`);
});

// Server-side variable to track the current maximum count
let currentMaxCount = 5; // Initialize with the default max from the app

// 4) Whenever Arduino prints a line, broadcast it to all connected WS clients
parser.on('data', line => {
  const msg = line.trim();
  console.log('⇐ From Arduino:', msg);
  wss.clients.forEach(ws => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(msg);
    }
  });
});

// 5) When a client connects via WebSocket…
wss.on('connection', ws => {
  console.log('⇒ App connected');

  // 5a) Forward any message from the app to the Arduino
  ws.on('message', msg => {
    console.log('⇒ To Arduino:', msg);
    const message = msg.toString().trim(); // Convert message to string and trim whitespace

    if (message.startsWith('MAX:')) {
      const newMax = parseInt(message.split(':')[1], 10);
      if (!isNaN(newMax) && newMax > 0) {
        const additionalCount = newMax - currentMaxCount;
        currentMaxCount = newMax;
        console.log(`Updated server max count to: ${currentMaxCount}`);
        // Send the new max to Arduino
        port.write(`MAX:${newMax}\n`, err => {
          if (err) console.error('Error writing MAX to serial:', err.message);
        });
        // If additional people were added, tell Arduino to open door
        if (additionalCount > 0) {
            console.log(`Sending OPEN_DOOR:${additionalCount} to Arduino`);
            port.write(`OPEN_DOOR:${additionalCount}\n`, err => {
                if (err) console.error('Error writing OPEN_DOOR to serial:', err.message);
            });
        }
      }
    } else if (message === 'RESET') {
        console.log('Received RESET from app, forwarding to Arduino');
        port.write('RESET\n', err => {
            if (err) console.error('Error writing RESET to serial:', err.message);
        });
         // Reset server-side max count on app reset (optional, depending on desired behavior)
        // currentMaxCount = 5; // Or some default
    } else {
        // Forward other messages from app to Arduino
        port.write(message + '\n', err => {
            if (err) console.error('Error writing message to serial:', err.message);
        });
    }
  });

  ws.on('close', () => console.log('⇐ App disconnected'));
  ws.on('error', err => console.error('WebSocket error:', err));
});
