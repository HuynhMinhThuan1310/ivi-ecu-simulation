// ==========================================
// 0. KẾT NỐI SOCKET.IO VỀ SERVER
// ==========================================
const socket = io();

// Biến lưu tốc độ thời gian thực từ biến trở ESP32
let carSpeedKmh = 0;
socket.on('speed_update', (data) => {
    carSpeedKmh = data.speed || 0;
});

// Lắng nghe GPS từ thiết bị khác (nếu có)
socket.on('gps_update', (data) => {
    if (isDriving) return; // Đang chạy mô phỏng thì không bị gián đoạn
    carLocation = [data.lat, data.lng];
    if (carMarker) {
        carMarker.setLatLng(carLocation);
        map.panTo(carLocation);
    }
});

// ==========================================
// 1. KHỞI TẠO BẢN ĐỒ LEAFLET
// ==========================================
const defaultLocation = [16.0738, 108.1499]; // Tọa độ Đà Nẵng (DUT)
const map = L.map('map', {
    zoomControl: false
}).setView(defaultLocation, 16);

L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 19
}).addTo(map);

L.control.zoom({ position: 'bottomright' }).addTo(map);

const hudPanel = document.querySelector('.hud-panel');
if (hudPanel) {
    L.DomEvent.disableClickPropagation(hudPanel);
    L.DomEvent.disableScrollPropagation(hudPanel);
}

// ==========================================
// 2. KHỞI TẠO ICON XE Ô TÔ THỰC TẾ & BIẾN TRẠNG THÁI
// ==========================================
let carLocation = defaultLocation;
let carMarker = null;
let destinationMarker = null;
let currentRouteLayer = null;

let activeRoutePoints = [];
let currentSegmentIndex = 0;
let isDriving = false;

// Icon chiếc xe ô tô Sedan thực tế (chuẩn kích thước & bóng đổ tự nhiên)
const carIcon = L.divIcon({
    className: 'car-marker-container',
    html: `
        <div id="car-rotate-icon" style="
            width: 26px;
            height: 52px;
            display: flex;
            align-items: center;
            justify-content: center;
            transform-origin: center center;
            transition: transform 0.1s linear;
        ">
            <svg viewBox="0 0 30 60" width="26" height="52" xmlns="http://www.w3.org/2000/svg">
                <defs>
                    <filter id="realCarShadow" x="-20%" y="-20%" width="140%" height="140%">
                        <feDropShadow dx="0" dy="2" stdDeviation="2" flood-color="#000000" flood-opacity="0.5"/>
                    </filter>
                    <linearGradient id="bodyMetallic" x1="0" y1="0" x2="1" y2="0">
                        <stop offset="0%" stop-color="#1f2937"/>
                        <stop offset="25%" stop-color="#374151"/>
                        <stop offset="50%" stop-color="#6b7280"/>
                        <stop offset="75%" stop-color="#374151"/>
                        <stop offset="100%" stop-color="#1f2937"/>
                    </linearGradient>
                    <linearGradient id="glassShine" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="0%" stop-color="#0f172a"/>
                        <stop offset="100%" stop-color="#1e293b"/>
                    </linearGradient>
                </defs>

                <g filter="url(#realCarShadow)">
                    <!-- Gương chiếu hậu 2 bên -->
                    <rect x="2.5" y="18" width="3" height="4" rx="1.5" fill="#374151"/>
                    <rect x="24.5" y="18" width="3" height="4" rx="1.5" fill="#374151"/>

                    <!-- Thân xe ô tô thực tế -->
                    <path d="M 7 12 C 7 5, 23 5, 23 12 L 25 24 C 26 32, 26 44, 24 53 C 23 57, 7 57, 6 53 C 4 44, 4 32, 5 24 Z" 
                          fill="url(#bodyMetallic)" stroke="#111827" stroke-width="0.8"/>

                    <!-- Đường gân nắp capo -->
                    <line x1="10" y1="7" x2="11" y2="15" stroke="#4b5563" stroke-width="0.6"/>
                    <line x1="20" y1="7" x2="19" y2="15" stroke="#4b5563" stroke-width="0.6"/>

                    <!-- Kính chắn gió trước -->
                    <path d="M 8 17 L 22 17 L 20 25 L 10 25 Z" fill="url(#glassShine)"/>
                    <line x1="11" y1="19" x2="16" y2="23" stroke="#94a3b8" stroke-width="0.7" opacity="0.6"/>

                    <!-- Nóc xe / Cửa sổ trời -->
                    <rect x="10" y="26" width="10" height="15" rx="1.5" fill="#111827"/>

                    <!-- Kính sau -->
                    <path d="M 10 42 L 20 42 L 21 47 L 9 47 Z" fill="url(#glassShine)"/>

                    <!-- Đèn pha trước LED -->
                    <path d="M 7 8 C 8 6, 11 6, 11 8 Z" fill="#fef08a"/>
                    <path d="M 19 8 C 19 6, 22 6, 23 8 Z" fill="#fef08a"/>

                    <!-- Đèn hậu LED đỏ -->
                    <rect x="7.5" y="55" width="5" height="1.5" rx="0.5" fill="#ef4444"/>
                    <rect x="17.5" y="55" width="5" height="1.5" rx="0.5" fill="#ef4444"/>
                </g>
            </svg>
        </div>
    `,
    iconSize: [26, 52],
    iconAnchor: [13, 26]
});

const destIcon = L.divIcon({
    className: 'dest-marker-container',
    html: `
        <div style="
            width: 26px;
            height: 26px;
            background: #ef4444;
            border: 2px solid #ffffff;
            border-radius: 50%;
            box-shadow: 0 0 15px #ef4444;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 14px;
        ">🏁</div>
    `,
    iconSize: [26, 26],
    iconAnchor: [13, 26]
});

// ==========================================
// 3. TẠO NÚT BẮT ĐẦU / TẠM DỪNG / HỦY TRÊN HUD
// ==========================================
let navControlBox = document.getElementById('nav-control-box');
if (!navControlBox && hudPanel) {
    navControlBox = document.createElement('div');
    navControlBox.id = 'nav-control-box';
    navControlBox.style.cssText = 'display:none; gap:8px; margin-top:10px;';
    navControlBox.innerHTML = `
        <button id="btn-start-nav" class="btn-ctrl" style="flex:1; background:rgba(0,240,255,0.25); border:1px solid #00f0ff; color:#00f0ff; font-weight:700; padding:9px; border-radius:8px; cursor:pointer;">🚀 BẮT ĐẦU ĐI</button>
        <button id="btn-cancel-nav" class="btn-ctrl" style="flex:1; background:rgba(239,68,68,0.2); border:1px solid #ef4444; color:#ef4444; font-weight:700; padding:9px; border-radius:8px; cursor:pointer;">✕ HỦY</button>
    `;
    hudPanel.appendChild(navControlBox);

    document.getElementById('btn-start-nav').addEventListener('click', toggleDriving);
    document.getElementById('btn-cancel-nav').addEventListener('click', cancelDriving);
}

// ==========================================
// 4. ĐỊNH VỊ GPS BAN ĐẦU (CHỈ LẤY 1 LẦN ĐẦU)
// ==========================================
const navText = document.getElementById('nav-text');
const distVal = document.getElementById('dist-val');
const timeVal = document.getElementById('time-val');

// Đặt marker xe ban đầu
carMarker = L.marker(carLocation, { icon: carIcon, zIndexOffset: 1000 }).addTo(map);

if ("geolocation" in navigator) {
    navigator.geolocation.getCurrentPosition(
        (position) => {
            if (isDriving) return;
            const lat = position.coords.latitude;
            const lng = position.coords.longitude;
            carLocation = [lat, lng];
            carMarker.setLatLng(carLocation);
            map.panTo(carLocation);
            socket.emit('gps_update', { lat, lng });
        },
        (error) => {
            console.warn("Không lấy được GPS:", error.message);
        },
        { enableHighAccuracy: true, timeout: 5000 }
    );
}

// ==========================================
// 5. CHỌN ĐIỂM ĐẾN & TÍNH TOÁN TUYẾN ĐƯỜNG
// ==========================================
map.on('click', function(e) {
    if (isDriving) return;

    const destLocation = [e.latlng.lat, e.latlng.lng];

    if (destinationMarker) {
        destinationMarker.setLatLng(destLocation);
    } else {
        destinationMarker = L.marker(destLocation, { icon: destIcon }).addTo(map);
    }

    navText.innerText = "Đang tính toán lộ trình...";
    const currentPos = carMarker ? [carMarker.getLatLng().lat, carMarker.getLatLng().lng] : carLocation;
    calculateRoute(currentPos, destLocation);
});

async function calculateRoute(start, end) {
    const url = `https://router.project-osrm.org/route/v1/driving/${start[1]},${start[0]};${end[1]},${end[0]}?overview=full&geometries=geojson`;

    try {
        const response = await fetch(url);
        const data = await response.json();

        if (data.code !== 'Ok' || !data.routes || data.routes.length === 0) {
            navText.innerText = "Không tìm thấy đường đi tới điểm này";
            return;
        }

        const route = data.routes[0];
        const distanceKm = (route.distance / 1000).toFixed(1);
        const durationMin = Math.max(1, Math.round(route.duration / 60));

        distVal.innerText = `${distanceKm} km`;
        timeVal.innerText = `${durationMin} phút`;
        navText.innerHTML = `Lộ trình: <strong>${distanceKm} km</strong> | Bấm BẮT ĐẦU`;

        activeRoutePoints = route.geometry.coordinates.map(c => L.latLng(c[1], c[0]));
        currentSegmentIndex = 0;

        if (currentRouteLayer) {
            map.removeLayer(currentRouteLayer);
        }

        currentRouteLayer = L.geoJSON(route.geometry, {
            style: {
                color: '#00f0ff',
                weight: 6,
                opacity: 0.9,
                lineCap: 'round',
                lineJoin: 'round'
            }
        }).addTo(map);

        map.fitBounds(currentRouteLayer.getBounds(), { padding: [80, 80] });

        if (navControlBox) {
            navControlBox.style.display = 'flex';
            const btnStart = document.getElementById('btn-start-nav');
            btnStart.style.display = 'block';
            btnStart.innerHTML = "🚀 BẮT ĐẦU ĐI";
            btnStart.style.background = "rgba(0, 240, 255, 0.25)";
            btnStart.style.borderColor = "#00f0ff";
            btnStart.style.color = "#00f0ff";
            document.getElementById('btn-cancel-nav').innerText = '✕ HỦY';
        }

    } catch (err) {
        console.error("Lỗi khi gọi OSRM:", err);
        navText.innerText = "Lỗi kết nối máy chủ dẫn đường";
    }
}

// Bắt đầu hoặc Tạm dừng / Tiếp tục lái xe (KHÔNG BỊ QUAY VỀ BAN ĐẦU)
function toggleDriving() {
    if (!activeRoutePoints || activeRoutePoints.length < 2) return;
    
    const btnStart = document.getElementById('btn-start-nav');

    if (!isDriving) {
        // Bắt đầu hoặc TIẾP TỤC chạy từ vị trí hiện tại
        isDriving = true;
        btnStart.innerHTML = "⏸ TẠM DỪNG";
        btnStart.style.background = "rgba(245, 158, 11, 0.25)";
        btnStart.style.borderColor = "#f59e0b";
        btnStart.style.color = "#fbbf24";
        navText.innerHTML = "🏎️ Đang dẫn đường: <strong style='color:#00f0ff;'>Vặn biến trở để chạy</strong>";

        // Phóng to cận cảnh mặt đường (Zoom level 18) y như thật
        map.setView(carMarker.getLatLng(), 18, { animate: true, duration: 1.0 });
    } else {
        // TẠM DỪNG XE NGAY TẠI CHỖ ĐANG ĐỨNG
        isDriving = false;
        btnStart.innerHTML = "▶ TIẾP TỤC";
        btnStart.style.background = "rgba(0, 240, 255, 0.25)";
        btnStart.style.borderColor = "#00f0ff";
        btnStart.style.color = "#00f0ff";
        navText.innerHTML = "⏸ <span style='color:#f59e0b;'>Đã tạm dừng xe</span> (Bấm Tiếp tục để chạy tiếp)";
    }
}

// Hủy hoàn toàn lộ trình: Dừng xe và GIỮ NGUYÊN VỊ TRÍ HIỆN TẠI
function cancelDriving() {
    isDriving = false;

    // Lưu lại vị trí xe đang dừng để lộ trình tiếp theo tính từ đây
    if (carMarker) {
        const stopPos = carMarker.getLatLng();
        carLocation = [stopPos.lat, stopPos.lng];
    }

    activeRoutePoints = [];
    currentSegmentIndex = 0;
    if (currentRouteLayer) map.removeLayer(currentRouteLayer);
    if (destinationMarker) map.removeLayer(destinationMarker);
    if (navControlBox) navControlBox.style.display = 'none';

    const btnStart = document.getElementById('btn-start-nav');
    if (btnStart) {
        btnStart.innerHTML = "🚀 BẮT ĐẦU ĐI";
        btnStart.style.background = "rgba(0, 240, 255, 0.25)";
        btnStart.style.borderColor = "#00f0ff";
        btnStart.style.color = "#00f0ff";
    }

    distVal.innerText = '-- km';
    timeVal.innerText = '-- phút';
    navText.innerText = "👉 Đã dừng xe. Chạm bản đồ để chọn điểm đến mới";

    // Zoom vừa phải bao quát (Zoom level 16)
    map.setView(carMarker.getLatLng(), 16, { animate: true, duration: 1.0 });
}

// ==========================================
// 6. THUẬT TOÁN XOAY ĐẦU XE & DI CHUYỂN THEO BIẾN TRỞ
// ==========================================
function getBearing(lat1, lon1, lat2, lon2) {
    const toRad = d => d * Math.PI / 180;
    const toDeg = r => r * 180 / Math.PI;
    const dLon = toRad(lon2 - lon1);
    const y = Math.sin(dLon) * Math.cos(toRad(lat2));
    const x = Math.cos(toRad(lat1)) * Math.sin(toRad(lat2)) -
              Math.sin(toRad(lat1)) * Math.cos(toRad(lat2)) * Math.cos(dLon);
    return (toDeg(Math.atan2(y, x)) + 360) % 360;
}

let lastFrameTime = performance.now();

function carPhysicsLoop() {
    requestAnimationFrame(carPhysicsLoop);
    const now = performance.now();
    const dt = (now - lastFrameTime) / 1000;
    lastFrameTime = now;

    if (!isDriving || !activeRoutePoints || activeRoutePoints.length < 2) return;

    // Khi xe đi hết toàn bộ lộ trình
    if (currentSegmentIndex >= activeRoutePoints.length - 1) {
        navText.innerHTML = "🎉 <strong style='color:#22c55e;'>BẠN ĐÃ ĐẾN NƠI!</strong>";
        isDriving = false;
        const btnStart = document.getElementById('btn-start-nav');
        if (btnStart) btnStart.style.display = 'none';
        document.getElementById('btn-cancel-nav').innerText = 'ĐÓNG';
        return;
    }

    // Đổi tốc độ từ km/h (biến trở ESP32) sang m/s
    const speedMs = (carSpeedKmh * 1000) / 3600;

    if (speedMs < 0.2) {
        // Biến trở = 0 km/h -> dừng xe
        navText.innerHTML = "🚦 <span style='color:#f59e0b;'>Đang dừng xe</span> (Vặn ga để chạy tiếp)";
        return;
    }

    navText.innerHTML = `Đang lái: <strong style="color:#00f0ff;">${Math.round(carSpeedKmh)} km/h</strong>`;

    let moveDistance = speedMs * dt; // Quãng đường mét đi được trong frame

    while (moveDistance > 0 && currentSegmentIndex < activeRoutePoints.length - 1) {
        const p1 = carMarker.getLatLng();
        const p2 = activeRoutePoints[currentSegmentIndex + 1];
        const segDist = p1.distanceTo(p2);

        if (moveDistance >= segDist) {
            moveDistance -= segDist;
            currentSegmentIndex++;
            carMarker.setLatLng(p2);
        } else {
            const ratio = moveDistance / segDist;
            const newLat = p1.lat + (p2.lat - p1.lat) * ratio;
            const newLng = p1.lng + (p2.lng - p1.lng) * ratio;

            // Xoay đầu xe theo tim đường khúc cua
            const heading = getBearing(p1.lat, p1.lng, p2.lat, p2.lng);
            const carEl = document.getElementById('car-rotate-icon');
            if (carEl) {
                carEl.style.transform = `rotate(${heading}deg)`;
            }

            carMarker.setLatLng([newLat, newLng]);
            moveDistance = 0;

            carLocation = [newLat, newLng];
            socket.emit('gps_update', { lat: newLat, lng: newLng, heading: heading });
        }
    }

    // Camera bám theo xe
    map.panTo(carMarker.getLatLng(), { animate: false });
}

carPhysicsLoop();

// ==========================================
// 7. TÌM KIẾM ĐỊA ĐIỂM (GEOCODING)
// ==========================================
const searchInput = document.getElementById('search-input');
const searchResults = document.getElementById('search-results');

async function searchLocation(query) {
    if (!query || query.length < 2) {
        if (searchResults) searchResults.style.display = 'none';
        return;
    }

    const [lat, lng] = carLocation;
    const delta = 0.5;
    const viewbox = `${lng - delta},${lat + delta},${lng + delta},${lat - delta}`;

    const url = `https://nominatim.openstreetmap.org/search` +
        `?q=${encodeURIComponent(query)}` +
        `&format=json` +
        `&limit=5` +
        `&viewbox=${viewbox}` +
        `&countrycodes=vn`;

    try {
        const response = await fetch(url, {
            headers: { 'Accept-Language': 'vi' }
        });
        const places = await response.json();
        renderSearchResults(places);
    } catch (err) {
        console.error('Lỗi geocoding:', err);
    }
}

function renderSearchResults(places) {
    if (!searchResults) return;
    searchResults.innerHTML = '';

    if (places.length === 0) {
        searchResults.style.display = 'block';
        searchResults.innerHTML = `
            <div class="search-result-item" style="color:#64748b;">
                Không tìm thấy địa điểm
            </div>`;
        return;
    }

    searchResults.style.display = 'block';
    places.forEach(place => {
        const item = document.createElement('div');
        item.className = 'search-result-item';

        const shortName = place.display_name.split(',').slice(0, 3).join(',');
        item.textContent = shortName;
        item.title = place.display_name;

        item.addEventListener('click', () => {
            const destLat = parseFloat(place.lat);
            const destLng = parseFloat(place.lon);
            const destLocation = [destLat, destLng];

            if (destinationMarker) {
                destinationMarker.setLatLng(destLocation);
            } else {
                destinationMarker = L.marker(destLocation, { icon: destIcon }).addTo(map);
            }

            if (searchInput) searchInput.value = place.display_name.split(',')[0];
            searchResults.style.display = 'none';
            navText.innerText = 'Đang tính toán lộ trình...';
            const currentPos = carMarker ? [carMarker.getLatLng().lat, carMarker.getLatLng().lng] : carLocation;
            calculateRoute(currentPos, destLocation);
        });

        searchResults.appendChild(item);
    });
}

let searchTimer = null;
if (searchInput) {
    searchInput.addEventListener('input', () => {
        clearTimeout(searchTimer);
        searchTimer = setTimeout(() => {
            searchLocation(searchInput.value.trim());
        }, 400);
    });

    searchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            clearTimeout(searchTimer);
            searchLocation(searchInput.value.trim());
        }
    });
}

document.addEventListener('click', (e) => {
    if (searchResults && !e.target.closest('.hud-panel')) {
        searchResults.style.display = 'none';
    }
});