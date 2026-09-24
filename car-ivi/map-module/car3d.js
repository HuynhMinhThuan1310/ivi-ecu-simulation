// ==========================================
// CAR3D.JS - 3D CAD BLUEPRINT ĐIỀU KHIỂN ĐỘC LẬP
// Tinh chỉnh: Xe to hơn, Cửa mở bung ra ngoài, Capo & Cốp nổi bật
// ==========================================

let scene, camera, renderer, carModel;
let headlightBeams = [];
const carDoors = {};
let hoodObj, trunkObj;

const carActuators = {
    fl: false,    // Cửa lái
    fr: false,    // Cửa phụ
    rl: false,    // Cửa sau trái
    rr: false,    // Cửa sau phải
    hood: false,  // Capo
    trunk: false, // Cốp
    lights: false,// Đèn pha
    locked: true
};

function initCar3D() {
    const container = document.getElementById('car-3d-viewport');
    if (!container) return;

    const width = container.clientWidth;
    const height = container.clientHeight;

    // 1. Scene
    scene = new THREE.Scene();

    // 2. Camera Top-Down - ĐƯA CAMERA LẠI GẦN ĐỂ XE TO HƠN (từ 9.5 xuống 7.0)
    camera = new THREE.PerspectiveCamera(36, width / height, 0.1, 100);
    camera.position.set(0, 9.0, 0.05); // Xe to rõ ràng hơn
    camera.up.set(0, 0, -1);           // Đầu xe hướng lên trên
    camera.lookAt(0, 0, 0);

    // 3. Renderer
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    container.innerHTML = '';
    container.appendChild(renderer.domElement);

    // 4. Lưới kỹ thuật mờ ảo dưới sàn
    const grid = new THREE.GridHelper(10, 20, 0x00f0ff, 0x051322);
    grid.position.y = -0.05;
    scene.add(grid);

    // 5. TẢI VÀ LỌC NÉT CAD XE THẬT
    const dracoLoader = new THREE.DRACOLoader();
    dracoLoader.setDecoderPath('https://www.gstatic.com/draco/versioned/decoders/1.4.3/');

    const loader = new THREE.GLTFLoader();
    loader.setDRACOLoader(dracoLoader);

    loader.load('assets/models/car.glb', (gltf) => {
        carModel = gltf.scene;

        carModel.traverse((child) => {
            if (child.isMesh) {
                // Thuật toán lọc góc cạnh CAD
                const edgesGeo = new THREE.EdgesGeometry(child.geometry, 24);
                const edgesMat = new THREE.LineBasicMaterial({
                    color: 0x00f0ff,
                    transparent: true,
                    opacity: 0.75
                });
                child.add(new THREE.LineSegments(edgesGeo, edgesMat));

                // Lớp vỏ sẫm màu trong suốt tạo chiều sâu
                child.material = new THREE.MeshBasicMaterial({
                    color: 0x020b17,
                    transparent: true,
                    opacity: 0.65,
                    depthWrite: true
                });
            }
        });

        carModel.position.set(0, 0, 0);
        scene.add(carModel);

        // Tạo cánh cửa, capo, cốp và đèn pha 3D
        buildInteractive3DParts();

        const loadingEl = document.getElementById('car-loading');
        if (loadingEl) loadingEl.style.display = 'none';

        renderer.render(scene, camera);
    }, undefined, (err) => {
        console.error("Lỗi nạp xe 3D:", err);
    });

    window.addEventListener('resize', () => {
        const w = container.clientWidth;
        const h = container.clientHeight;
        camera.aspect = w / h;
        camera.updateProjectionMatrix();
        renderer.setSize(w, h);
        renderer.render(scene, camera);
    });
}

// ── DỰNG CÁNH CỬA, CAPO, CỐP BUNG MỞ RA NGOÀI ──
function buildInteractive3DParts() {
    const hazardLineMat = new THREE.LineBasicMaterial({ color: 0xf59e0b, linewidth: 2 });
    const hazardFillMat = new THREE.MeshBasicMaterial({
        color: 0xf59e0b,
        transparent: true,
        opacity: 0.35,
        side: THREE.DoubleSide
    });

    // Hàm tạo cánh cửa 3D với độ dày và viền cảnh báo nổi bật
    function createDoorWing(isLeft, hingeZ, lengthZ) {
        const doorGroup = new THREE.Group();
        const posX = isLeft ? -0.98 : 0.98;
        doorGroup.position.set(posX, 0.55, hingeZ);

        // Khối cửa có bề dày thực tế
        const wingGeo = new THREE.BoxGeometry(0.06, 0.45, lengthZ);
        wingGeo.translate(0, 0, lengthZ / 2); // Đặt bản lề ở mép trước

        const wingMesh = new THREE.Mesh(wingGeo, hazardFillMat);
        wingMesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(wingGeo), hazardLineMat));

        doorGroup.add(wingMesh);
        doorGroup.visible = false;
        scene.add(doorGroup);
        return doorGroup;
    }

    // 4 Cánh cửa 3D
    carDoors.fl = createDoorWing(true, -0.65, 0.95);  // Cửa lái
    carDoors.fr = createDoorWing(false, -0.65, 0.95); // Cửa phụ
    carDoors.rl = createDoorWing(true, 0.35, 0.85);   // Sau trái
    carDoors.rr = createDoorWing(false, 0.35, 0.85);  // Sau phải

    // ── NẮP CAPO 3D (ĐẶT CAO HƠN VỎ XE ĐỂ NHÌN TỪ TRÊN XUỐNG THẤY NGAY) ──
    hoodObj = new THREE.Group();
    hoodObj.position.set(0, 0.95, -0.7); // Bản lề chân kính lái
    const hoodGeo = new THREE.BoxGeometry(1.45, 0.08, 1.25);
    hoodGeo.translate(0, 0, -0.62); // Vươn ra phía trước đầu xe

    const hoodMesh = new THREE.Mesh(hoodGeo, hazardFillMat);
    hoodMesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(hoodGeo), hazardLineMat));
    hoodObj.add(hoodMesh);
    hoodObj.visible = false;
    scene.add(hoodObj);

    // ── CỐP SAU 3D (ĐẶT CAO HƠN VỎ XE) ──
    trunkObj = new THREE.Group();
    trunkObj.position.set(0, 0.95, 0.65); // Bản lề chân kính sau
    const trunkGeo = new THREE.BoxGeometry(1.35, 0.08, 1.35);
    trunkGeo.translate(0, 0, 0.68); // Vươn ra phía sau đuôi xe

    const trunkMesh = new THREE.Mesh(trunkGeo, hazardFillMat);
    trunkMesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(trunkGeo), hazardLineMat));
    trunkObj.add(trunkMesh);
    trunkObj.visible = false;
    scene.add(trunkObj);

    // ── ĐÈN PHA 3D ──
    const beamGeo = new THREE.ConeGeometry(0.7, 3.8, 16);
    beamGeo.rotateX(-Math.PI / 2);
    beamGeo.translate(0, 0, -1.9);
    const beamMat = new THREE.MeshBasicMaterial({ color: 0xfef08a, transparent: true, opacity: 0, depthWrite: false });

    const leftBeam = new THREE.Mesh(beamGeo, beamMat);
    leftBeam.position.set(-0.6, 0.4, -2.0);
    scene.add(leftBeam);

    const rightBeam = new THREE.Mesh(beamGeo, beamMat.clone());
    rightBeam.position.set(0.6, 0.4, -2.0);
    scene.add(rightBeam);

    headlightBeams = [leftBeam, rightBeam];
}

// ── CÁC HÀM TƯƠNG TÁC ĐỘC LẬP ──

// Mở cửa vung RA PHÍA NGOÀI
function toggleDoor(doorKey) {
    carActuators[doorKey] = !carActuators[doorKey];
    const group = carDoors[doorKey];
    const btn = document.getElementById(`btn-${doorKey}`);

    if (group) {
        group.visible = carActuators[doorKey];
        // Đảo dấu: Bên trái xoay ÂM (vung ra trái), Bên phải xoay DƯƠNG (vung ra phải)
        const angle = (doorKey === 'fl' || doorKey === 'rl') ? -Math.PI / 4.2 : Math.PI / 4.2;
        group.rotation.y = carActuators[doorKey] ? angle : 0;
    }
    if (btn) btn.classList.toggle('active', carActuators[doorKey]);

    updateHUDStatus();
    renderer.render(scene, camera);
}

// Bật nắp Capo (Hất ngược lên và nâng cao)
function toggleHood() {
    carActuators.hood = !carActuators.hood;
    if (hoodObj) {
        hoodObj.visible = carActuators.hood;
        // Nâng cao và hất nghiêng góc 30 độ
        hoodObj.rotation.x = carActuators.hood ? Math.PI / 5.5 : 0;
        hoodObj.position.y = carActuators.hood ? 1.15 : 0.95;
    }
    const btn = document.getElementById('btn-hood');
    if (btn) btn.classList.toggle('active', carActuators.hood);

    updateHUDStatus();
    renderer.render(scene, camera);
}

// Mở Cốp Sau (Hất ngược lên đuôi xe)
function toggleTrunk() {
    carActuators.trunk = !carActuators.trunk;
    if (trunkObj) {
        trunkObj.visible = carActuators.trunk;
        trunkObj.rotation.x = carActuators.trunk ? -Math.PI / 5.5 : 0;
        trunkObj.position.y = carActuators.trunk ? 1.15 : 0.95;
    }
    const btn = document.getElementById('btn-trunk');
    if (btn) btn.classList.toggle('active', carActuators.trunk);

    updateHUDStatus();
    renderer.render(scene, camera);
}

// Bật/Tắt Đèn Pha 3D
function toggleLights() {
    carActuators.lights = !carActuators.lights;
    const targetOpacity = carActuators.lights ? 0.6 : 0;
    headlightBeams.forEach(b => { b.material.opacity = targetOpacity; });

    const btn = document.getElementById('btn-lights');
    if (btn) btn.classList.toggle('active', carActuators.lights);

    renderer.render(scene, camera);
}

// Mở/Đóng 4 cửa
function toggleAllDoors() {
    const doors = ['fl', 'fr', 'rl', 'rr'];
    const anyClosed = doors.some(d => !carActuators[d]);
    doors.forEach(d => {
        if (carActuators[d] !== anyClosed) toggleDoor(d);
    });
}

// Khóa tổng
function toggleMasterLock() {
    carActuators.locked = !carActuators.locked;
    if (carActuators.locked) {
        ['fl', 'fr', 'rl', 'rr'].forEach(d => { if (carActuators[d]) toggleDoor(d); });
        if (carActuators.hood) toggleHood();
        if (carActuators.trunk) toggleTrunk();
    }
    updateHUDStatus();
}

function updateHUDStatus() {
    const pill = document.getElementById('lock-pill');
    const text = document.getElementById('lock-text');
    const anyOpen = carActuators.fl || carActuators.fr || carActuators.rl || carActuators.rr || carActuators.hood || carActuators.trunk;

    if (pill && text) {
        if (anyOpen) {
            pill.className = 'status-pill warning';
            text.innerText = 'CỬA / CAPO ĐANG MỞ';
        } else if (!carActuators.locked) {
            pill.className = 'status-pill warning';
            text.innerText = 'ĐÃ MỞ KHÓA';
        } else {
            pill.className = 'status-pill secure';
            text.innerText = 'AN TOÀN - ĐÃ KHÓA';
        }
    }
}

document.addEventListener('DOMContentLoaded', initCar3D);