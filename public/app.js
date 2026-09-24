// AUTOMOTIVE DIGITAL COCKPIT LOGIC (60 FPS)
let ws = null;
let currentSpeed = 0;
let targetSpeed = 0;
let currentRPM = 800;
let targetRPM = 800;
let currentGear = 'D';
let frameCount = 0;
let lastFpsTime = performance.now();

const canvas = document.getElementById('gaugeCanvas');
const ctx = canvas.getContext('2d');

function initWebSocket() {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const wsUrl = `${protocol}//${window.location.host}/ws`;
  
  ws = new WebSocket(wsUrl);
  
  ws.onopen = () => {
    document.getElementById('connStatus').textContent = 'CONNECTED (BBB)';
    document.getElementById('connStatus').className = 'connection-status online';
  };
  
  ws.onclose = () => {
    document.getElementById('connStatus').textContent = 'DISCONNECTED';
    document.getElementById('connStatus').className = 'connection-status offline';
    setTimeout(initWebSocket, 2000);
  };
  
  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      updateTelemetry(data);
    } catch (e) {
      console.error(e);
    }
  };
}

function updateTelemetry(data) {
  targetSpeed = data.speed || 0;
  targetRPM = data.rpm || 800;
  currentGear = data.gear || 'D';
  
  // Battery & Range
  document.getElementById('socVal').textContent = data.soc;
  document.getElementById('batteryBar').style.width = `${data.soc}%`;
  document.getElementById('rangeVal').textContent = data.range_km;
  document.getElementById('powerVal').textContent = `${data.power_kw >= 0 ? '+' : ''}${data.power_kw} kW`;
  document.getElementById('tempVal').textContent = `${data.temp_c} °C`;
  document.getElementById('odoVal').textContent = `${data.odometer.toLocaleString()} km`;
  
  // Throttle
  document.getElementById('throttleFill').style.width = `${data.throttle}%`;
  
  // Gears
  ['P', 'R', 'N', 'D'].forEach(g => {
    const el = document.getElementById(`gear-${g}`);
    if (el) el.className = (g === currentGear) ? 'gear-badge active' : 'gear-badge';
  });
  
  // Drive Mode
  document.body.className = `mode-${data.drive_mode.toLowerCase()}`;
  ['ECO', 'COMFORT', 'SPORT'].forEach(m => {
    const el = document.getElementById(`btnMode${m.charAt(0) + m.slice(1).toLowerCase()}`);
    if (el) el.className = (m === data.drive_mode) ? 'btn-mode active' : 'btn-mode';
  });
  
  // Blinkers & Lock
  const tl = document.getElementById('turnLeft');
  const tr = document.getElementById('turnRight');
  if (tl) tl.className = data.lights.left ? 'turn-arrow left active' : 'turn-arrow left';
  if (tr) tr.className = data.lights.right ? 'turn-arrow right active' : 'turn-arrow right';
  
  const lockEl = document.getElementById('lockIndicator');
  if (lockEl) {
    lockEl.textContent = data.locked ? '🔒 LOCKED' : '🔓 UNLOCKED';
    lockEl.style.color = data.locked ? 'var(--accent-cyan)' : 'var(--accent-green)';
  }
  
  // Doors
  updateDoor('doorFL', data.doors.fl);
  updateDoor('doorFR', data.doors.fr);
  updateDoor('doorRL', data.doors.rl);
  updateDoor('doorRR', data.doors.rr);
  
  const openDoors = Object.entries(data.doors).filter(([k, v]) => v).map(([k]) => k.toUpperCase());
  document.getElementById('doorStatusText').textContent = openDoors.length === 0 ? 'All Doors Closed' : `Door Open: ${openDoors.join(', ')}`;
  
  // Headlight beam
  const beam = document.getElementById('lightBeam');
  if (beam) {
    beam.style.opacity = data.lights.low ? '0.5' : '0';
  }
  
  // CAN Bus inspector
  if (data.can_frames && data.can_frames.length > 0) {
    const stream = document.getElementById('canStream');
    stream.innerHTML = data.can_frames.map(f => {
      let cls = 'can-id-powertrain';
      if (f.id === '0x200') cls = 'can-id-battery';
      if (f.id === '0x300') cls = 'can-id-body';
      return `<div class="can-row"><span class="${cls}">[CAN ${f.id}]</span> DLC:${f.dlc} DATA: <b>${f.data}</b> &rarr; ${f.desc}</div>`;
    }).join('');
  }
  
  frameCount++;
}

function updateDoor(id, isOpen) {
  const el = document.getElementById(id);
  if (!el) return;
  if (isOpen) {
    el.classList.add('open');
    if (id === 'doorFL') { el.setAttribute('x1', '25'); el.setAttribute('x2', '40'); }
    if (id === 'doorFR') { el.setAttribute('x1', '175'); el.setAttribute('x2', '160'); }
    if (id === 'doorRL') { el.setAttribute('x1', '25'); el.setAttribute('x2', '40'); }
    if (id === 'doorRR') { el.setAttribute('x1', '175'); el.setAttribute('x2', '160'); }
  } else {
    el.classList.remove('open');
    if (id === 'doorFL' || id === 'doorRL') { el.setAttribute('x1', '40'); el.setAttribute('x2', '40'); }
    if (id === 'doorFR' || id === 'doorRR') { el.setAttribute('x1', '160'); el.setAttribute('x2', '160'); }
  }
}

function sendCmd(action, value = null) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action, value }));
  }
}

function toggleGear() {
  const order = ['P', 'R', 'N', 'D'];
  const next = order[(order.indexOf(currentGear) + 1) % order.length];
  sendCmd('gear', next);
}

// 60 FPS CANVAS GAUGE RENDERER
function renderGauge() {
  currentSpeed += (targetSpeed - currentSpeed) * 0.12;
  currentRPM += (targetRPM - currentRPM) * 0.12;
  
  document.getElementById('speedDisplay').textContent = Math.round(currentSpeed);
  document.getElementById('rpmDisplay').textContent = `${Math.round(currentRPM)} RPM`;
  
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  
  const cx = canvas.width / 2;
  const cy = canvas.height / 2 + 10;
  const radius = 150;
  
  const startAngle = 0.75 * Math.PI;
  const endAngle = 2.25 * Math.PI;
  const totalAngle = endAngle - startAngle;
  
  // Background Track Arc
  ctx.beginPath();
  ctx.arc(cx, cy, radius, startAngle, endAngle);
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.08)';
  ctx.lineWidth = 14;
  ctx.lineCap = 'round';
  ctx.stroke();
  
  // Active Speed Arc
  const maxDisplaySpeed = 160.0;
  const speedPct = Math.min(1.0, Math.max(0, currentSpeed / maxDisplaySpeed));
  const activeAngle = startAngle + speedPct * totalAngle;
  
  ctx.beginPath();
  ctx.arc(cx, cy, radius, startAngle, activeAngle);
  
  const computedColor = getComputedStyle(document.body).getPropertyValue('--gauge-color').trim() || '#38bdf8';
  ctx.strokeStyle = computedColor;
  ctx.lineWidth = 14;
  ctx.lineCap = 'round';
  ctx.shadowColor = computedColor;
  ctx.shadowBlur = 18;
  ctx.stroke();
  ctx.shadowBlur = 0;
  
  // Tick Marks
  for (let i = 0; i <= 16; i++) {
    const tickPct = i / 16;
    const angle = startAngle + tickPct * totalAngle;
    const isMajor = i % 4 === 0;
    const innerR = radius - (isMajor ? 26 : 18);
    const outerR = radius - 12;
    
    const x1 = cx + Math.cos(angle) * innerR;
    const y1 = cy + Math.sin(angle) * innerR;
    const x2 = cx + Math.cos(angle) * outerR;
    const y2 = cy + Math.sin(angle) * outerR;
    
    ctx.beginPath();
    ctx.moveTo(x1, y1);
    ctx.lineTo(x2, y2);
    ctx.strokeStyle = tickPct <= speedPct ? '#fff' : 'rgba(255, 255, 255, 0.2)';
    ctx.lineWidth = isMajor ? 3 : 1.5;
    ctx.stroke();
  }
  
  requestAnimationFrame(renderGauge);
}

// Clock update
setInterval(() => {
  const d = new Date();
  document.getElementById('clock').textContent = d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}, 1000);

// FPS Counter
setInterval(() => {
  const now = performance.now();
  const dt = (now - lastFpsTime) / 1000;
  const fps = Math.round(frameCount / dt);
  frameCount = 0;
  lastFpsTime = now;
  document.getElementById('canRate').textContent = `${fps * 3} CAN frames/sec`;
}, 1000);

window.onload = () => {
  initWebSocket();
  requestAnimationFrame(renderGauge);
};
