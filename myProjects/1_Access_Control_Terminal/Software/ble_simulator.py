import asyncio
import struct
from bleak import BleakScanner, BleakClient

# Cihazın BLE Servis ve RX UUID bilgileri
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E".lower()
CHAR_RX_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E".lower()

def calculate_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def build_provision_frame(token_hex_64: str) -> bytes:
    # 64 karakterlik HEX string'i 32 bytelık raw veriye çevir
    token_bytes = bytes.fromhex(token_hex_64)
    
    frame = bytearray()
    frame.append(0x4F) # SOF
    frame.append(0x02) # VER
    frame.append(0x01) # MSG_TYPE_REQUEST
    frame.append(0x02) # CMD_PROVISION (0x02)
    frame.extend(struct.pack('>H', 1)) # Seq = 1
    frame.extend(struct.pack('>H', len(token_bytes))) # Payload len = 32
    frame.append(0x00) # Status
    frame.extend(b'\x00' * 8) # Timestamp (Bos)
    frame.extend(token_bytes) # Payload (Token)
    
    # CRC Hesapla
    crc = calculate_crc16(frame)
    frame.extend(struct.pack('>H', crc))
    
    return bytes(frame)

async def main():
    print("Bluetooth tarafindaki Onloi cihazlari taraniyor...")
    devices = await BleakScanner.discover(timeout=5.0)
    target_device = None
    
    for d in devices:
        if d.name and "Onloi_" in d.name:
            target_device = d
            break
            
    if not target_device:
        print("HATA: Cihaz bulunamadi! Cihazin acik ve reklam(advertisement) yaptigindan emin olun.")
        return
        
    print(f"Cihaz Bulundu: {target_device.name} [{target_device.address}]")
    print("Baglaniliyor...")
    
    try:
        async with BleakClient(target_device.address) as client:
            print("Baglanti Basarili!")
            
            # Sunucudan alinmis gecerli bir token veya sahte 64 karakter token
            # Eğer gecerli tokenin varsa buraya yaz!
            my_token_hex = "11223344556677889900AABBCCDDEEFF11223344556677889900AABBCCDDEEFF"
            
            frame = build_provision_frame(my_token_hex)
            
            print(f"CMD_PROVISION paketi gonderiliyor... ({len(frame)} byte)")
            await client.write_gatt_char(CHAR_RX_UUID, frame, response=False)
            
            print("Provisioning Token cihaza basariyla gonderildi!")
            print("Lutfen ESP32 konsolundan cihazin HTTP sunucusuna ulasip ulasmadigini kontrol edin.")
            
    except Exception as e:
        print(f"Baglanti Hatasi: {e}")

if __name__ == "__main__":
    asyncio.run(main())
