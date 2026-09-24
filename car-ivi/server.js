// ==========================================
// SERVER.JS - Node.js Backend (HTTPS)
// Vị trí: ~/car-ivi/server.js
// ==========================================
const fs      = require('fs');
const https   = require('https');
const path    = require('path');
const express = require('express');
const { Server } = require('socket.io');

const app = express();

// ── SSL cert vẫn để trong map-module ──
const server = https.createServer({
    key:  fs.readFileSync(path.join(__dirname, 'map-module/key.pem')),
    cert: fs.readFileSync(path.join(__dirname, 'map-module/cert.pem'))
}, app);

const io  = new Server(server);
const PORT = 8080;

// ── Parse JSON body từ ESP32 ──
app.use(express.json());

// ------------------------------------------
// 1. SERVE CÁC FOLDER TĨNH
// ------------------------------------------

// Trang chủ → map-module
app.use('/', express.static(path.join(__dirname, 'map-module')));

// Trang đồng hồ → speedometer
app.use('/speedometer', express.static(path.join(__dirname, 'speedometer')));

// ------------------------------------------
// 2. API NHẬN TỐC ĐỘ TỪ ESP32
// ------------------------------------------
let currentSpeed = 0;

app.post('/speed', (req, res) => {
    currentSpeed = req.body.speed ?? 0;
    console.log(`[SPEED] ESP32 → ${currentSpeed} km/h`);
    io.emit('speed_update', { speed: currentSpeed });
    res.json({ ok: true });
});

// ------------------------------------------
// 3. XỬ LÝ SOCKET.IO (GPS + SPEED REALTIME)
// ------------------------------------------
io.on('connection', (socket) => {
    console.log(`[+] Kết nối: ${socket.id}`);

    // Gửi tốc độ hiện tại ngay khi client vừa kết nối
    socket.emit('speed_update', { speed: currentSpeed });

    socket.on('gps_update', (data) => {
        console.log(`[GPS] lat: ${data.lat}, lng: ${data.lng}`);
        socket.broadcast.emit('gps_update', data);
    });

    socket.on('disconnect', () => {
        console.log(`[-] Ngắt kết nối: ${socket.id}`);
    });
});

// ------------------------------------------
// 4. KHỞI ĐỘNG SERVER
// ------------------------------------------
server.listen(PORT, '0.0.0.0', () => {
    console.log(`=============================================`);
    console.log(`✅ IVI HTTPS Server đang chạy cổng: ${PORT}`);
    console.log(`🗺  Bản đồ:     https://localhost:${PORT}/`);
    console.log(`🏎  Speedometer: https://localhost:${PORT}/speedometer/`);
    console.log(`=============================================`);
});