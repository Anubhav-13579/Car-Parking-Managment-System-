const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = process.env.PORT || 3000;
const DASHBOARD_DIR = path.join(__dirname, 'dashboard');

const MIME_TYPES = {
  '.html': 'text/html',
  '.css': 'text/css',
  '.js': 'application/javascript',
  '.json': 'application/json',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.svg': 'image/svg+xml'
};

const server = http.createServer((req, res) => {
  let reqUrl = req.url === '/' ? '/index.html' : req.url;
  let filePath = path.join(DASHBOARD_DIR, reqUrl);

  const ext = path.extname(filePath);
  const contentType = MIME_TYPES[ext] || 'text/plain';

  fs.readFile(filePath, (err, content) => {
    if (err) {
      if (err.code === 'ENOENT') {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('404 Not Found');
      } else {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end(`Server Error: ${err.code}`);
      }
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content, 'utf-8');
    }
  });
});

server.listen(PORT, () => {
  console.log('========================================================================');
  console.log('  OS - 103: Smart Parking Management System Dashboard Server');
  console.log('  NIET B.Tech CSE (AI) Capstone Project - Group 8');
  console.log('========================================================================');
  console.log(`  Live Dashboard running at: http://localhost:${PORT}`);
  console.log('  Open this link in your browser to inspect real-time OS concurrency!');
  console.log('========================================================================');
});
