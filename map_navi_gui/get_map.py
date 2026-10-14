import math
import io
import struct
import requests
from PIL import Image

TARGET_W = 1024
TARGET_H = 600
ZOOM = 15

def get_current_location():
    try:
        print("[*] Dang tu dong do toa do GPS theo IP mang...")
        res = requests.get('https://ipinfo.io/json', timeout=5).json()
        loc = res['loc'].split(',')
        lat, lon = float(loc[0]), float(loc[1])
        city = res.get('city', '')
        region = res.get('region', '')
        print(f"[+] Vi tri phat hien: {city}, {region} ({lat}, {lon})")
        return lat, lon
    except Exception as e:
        print(f"[-] Khong lay duoc IP GPS ({e}), su dung toa do mac dinh (DUT Da Nang).")
        return 16.0738, 108.1499

def latlon_to_xy(lat, lon, zoom):
    n = 2.0 ** zoom
    x = (lon + 180.0) / 360.0 * n
    lat_rad = math.radians(lat)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x, y

def main():
    center_lat, center_lon = get_current_location()
    cx, cy = latlon_to_xy(center_lat, center_lon, ZOOM)
    center_px_x = cx * 256.0
    center_px_y = cy * 256.0
    
    min_px_x = center_px_x - TARGET_W / 2.0
    min_px_y = center_px_y - TARGET_H / 2.0
    
    tile_x_min = int(min_px_x // 256)
    tile_x_max = int((min_px_x + TARGET_W) // 256)
    tile_y_min = int(min_px_y // 256)
    tile_y_max = int((min_px_y + TARGET_H) // 256)
    
    stitched = Image.new('RGB', ((tile_x_max - tile_x_min + 1) * 256, (tile_y_max - tile_y_min + 1) * 256), (240, 240, 240))
    headers = {'User-Agent': 'CarIVI_BBB_Project/1.0'}
    
    print("[*] Dang tai cac manh ban do khu vuc tu OpenStreetMap...")
    for tx in range(tile_x_min, tile_x_max + 1):
        for ty in range(tile_y_min, tile_y_max + 1):
            url = f"https://tile.openstreetmap.org/{ZOOM}/{tx}/{ty}.png"
            try:
                r = requests.get(url, headers=headers, timeout=10)
                if r.status_code == 200:
                    tile_img = Image.open(io.BytesIO(r.content)).convert('RGB')
                    stitched.paste(tile_img, ((tx - tile_x_min) * 256, (ty - tile_y_min) * 256))
            except Exception:
                pass
                
    crop_x1 = int(min_px_x - tile_x_min * 256)
    crop_y1 = int(min_px_y - tile_y_min * 256)
    final_img = stitched.crop((crop_x1, crop_y1, crop_x1 + TARGET_W, crop_y1 + TARGET_H))
    
    print("[*] Dang chuyen doi dinh dang RGB565 Little-Endian...")
    raw_bytes = bytearray()
    pixels = final_img.load()
    for y in range(TARGET_H):
        for x in range(TARGET_W):
            r, g, b = pixels[x, y]
            rgb565 = (((r >> 3) & 0x1F) << 11) | (((g >> 2) & 0x3F) << 5) | ((b >> 3) & 0x1F)
            raw_bytes += struct.pack('<H', rgb565)
            
    with open("map_1024x600.raw", "wb") as f:
        f.write(raw_bytes)
    print(f"[OK] Da xuat xong file map_1024x600.raw ({len(raw_bytes)} bytes)!")

if __name__ == '__main__':
    main()
