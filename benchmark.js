const net = require('net');

const concurrent = 50;
const duration = 5; // seconds
let totalComplete = 0;
let completeRequestsSecond = 0;
let rateLimitedSecond = 0;
let errorsSecond = 0;
let totalErrors = 0;
let rateLimitedTotal = 0;
let activeClients = 0;

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
  console.log(`[START] Benchmarking on UDS socket: /tmp/http.sock`);
  console.log(`[CONFIG] Concurrency: ${concurrent}, Duration: ${duration} seconds\n`);
  for (let i = 0; i < concurrent; i++) {
    startClient();
  }
};

const startClient = () => {
  const client = net.createConnection('/tmp/http.sock');
  let dataBuffer = '';

  client.on('connect', () => {
    activeClients++;
    // Pipeline requests
    for(let j=0; j<10; j++) client.write(requestData);
  });

  client.on('data', (chunk) => {
    dataBuffer += chunk.toString();
    
    // Count HTTP 200 OK responses
    let idx;
    while ((idx = dataBuffer.indexOf('HTTP/1.1 200 OK')) !== -1) {
       totalComplete++;
       completeRequestsSecond++;
       dataBuffer = dataBuffer.substring(idx + 15);
       client.write(requestData);
    }
    
    // Count HTTP 429 Too Many Requests responses
    while ((idx = dataBuffer.indexOf('HTTP/1.1 429 Too Many Requests')) !== -1) {
       rateLimitedTotal++;
       rateLimitedSecond++;
       dataBuffer = dataBuffer.substring(idx + 30);
       client.write(requestData);
    }
    
    // Truncate to avoid memory leaks on weird chunks
    if (dataBuffer.length > 8192) {
      dataBuffer = dataBuffer.substring(dataBuffer.length - 1000);
    }
  });

  client.on('close', () => {
    activeClients--;
    startClient();
  });
  
  client.on('error', (e) => {
    errorsSecond++;
    totalErrors++;
  });
}

run();

let second = 0;
setInterval(() => {
  second++;
  console.log(`[s=${second}] 200 OK/s: ${completeRequestsSecond} | 429s/s: ${rateLimitedSecond} | Errors/s: ${errorsSecond} | Active Clients: ${activeClients}`);
  completeRequestsSecond = 0;
  rateLimitedSecond = 0;
  errorsSecond = 0;
}, 1000);

setTimeout(() => {
  console.log(`\n[DONE] Benchmark completed.`);
  console.log(`Total 200 OK: ${totalComplete}`);
  console.log(`Total 429s  : ${rateLimitedTotal}`);
  console.log(`Total Errors: ${totalErrors}`);
  process.exit(0);
}, duration * 1000);
