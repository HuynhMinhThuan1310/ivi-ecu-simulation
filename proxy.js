const net = require('net');

const LISTEN_PORT = 8080;
const TARGET_HOST = '192.168.18.130';
const TARGET_PORT = 8080;

const server = net.createServer((clientSocket) => {
    const targetSocket = net.connect(TARGET_PORT, TARGET_HOST, () => {
        clientSocket.pipe(targetSocket);
        targetSocket.pipe(clientSocket);
    });

    clientSocket.on('error', () => {});
    targetSocket.on('error', () => {});
});

server.on('error', (err) => {
    if (err.code === 'EADDRINUSE') {
        console.error(`[LỖI] Cổng ${LISTEN_PORT} đang có ứng dụng khác sử dụng!`);
    } else {
        console.error('[LỖI PROXY]:', err.message);
    }
});

server.listen(LISTEN_PORT, '0.0.0.0', () => {
    console.log('========================================================');
    console.log(`🚀 CẦU NỐI MẠNG (BRIDGE PROXY) ĐANG CHẠY THÀNH CÔNG!`);
    console.log(`📡 Đang đón kết nối cổng: ${LISTEN_PORT}`);
    console.log(`🎯 Chuyển tiếp thẳng vào Ubuntu VM: ${TARGET_HOST}:${TARGET_PORT}`);
    console.log(`📱 Trên điện thoại mở: https://192.168.1.10:${LISTEN_PORT}`);
    console.log('========================================================');
    console.log('>> Để cửa sổ này chạy khi test. Nhấn Ctrl + C để dừng.');
});
