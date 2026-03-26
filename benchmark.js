const net = require('net');

const concurrent = 50;
const duration = 5; // seconds
let completeRequests = 0;
let errors = 0;

const requestData = Buffer.from(
  'POST / HTTP/1.1\r\n' +
  'Host: localhost\r\n' +
  'Authorization: Bearer secret123\r\n' +
  'Content-Type: application/json\r\n' +
  'Content-Length: 17\r\n' +
  'Connection: keep-alive\r\n\r\n' +
  '{"hello":"world"}'
);

const run = () => {
  for (let i = 0; i < concurrent; i++) {
    startClient();
  }
};

const startClient = () => {
  const client = net.createConnection('/tmp/http.sock');
  let dataBuffer = '';

  client.on('connect', () => {
    // Pipeline requests
    for(let j=0; j<10; j++) client.write(requestData);
  });

  client.on('data', (chunk) => {
    dataBuffer += chunk.toString();
    // Count HTTP responses
    let idx;
    while ((idx = dataBuffer.indexOf('HTTP/1.1 200 OK')) !== -1) {
       completeRequests++;
       dataBuffer = dataBuffer.substring(idx + 15);
       client.write(requestData);
    }
  });

  client.on('end', () => {
    startClient();
  });
  
  client.on('error', (e) => {
    errors++;
  });
}

run();

setInterval(() => {
  console.log(`RPS: ${completeRequests}`);
  completeRequests = 0;
}, 1000);

setTimeout(() => {
  console.log(`Benchmark completed with ${errors} errors.`);
  process.exit(0);
}, (duration + 1) * 1000);
