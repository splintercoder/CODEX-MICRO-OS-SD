import cv2
import numpy as np
import urllib.request
import time

ESP32_TARGET_URL = "http://192.168.1.28/stream"
WINDOW_TITLE = "CODEX OS - Laser Blue Frame Monitor"
EXPECTED_BYTES = 76800

def run_bios_stream_monitor():
    print(f"Connecting to live target frame system at: {ESP32_TARGET_URL}")
    cv2.namedWindow(WINDOW_TITLE, cv2.WINDOW_NORMAL)
    
    # Establish a persistent urllib opener to reduce handshake overhead
    opener = urllib.request.build_opener()
    
    while True:
        try:
            # 1. Fetch exactly one frame from the ESP32 chunk server
            with opener.open(ESP32_TARGET_URL, timeout=2.0) as response:
                raw_data = b""
                while len(raw_data) < EXPECTED_BYTES:
                    packet = response.read(EXPECTED_BYTES - len(raw_data))
                    if not packet:
                        break
                    raw_data += packet
                
                # Check if we got a complete payload matrix
                if len(raw_data) == EXPECTED_BYTES:
                    frame = np.frombuffer(raw_data, dtype=np.uint8).reshape((240, 320))
                    
                    # Apply your custom Laser Blue phosphorus color profile
                    blue_channel = frame
                    green_channel = np.clip((frame // 2) + 64, 0, 255).astype(np.uint8) # Guard overflow
                    red_channel = np.zeros_like(frame)
                    
                    colored_frame = cv2.merge([blue_channel, green_channel, red_channel])
                    cv2.imshow(WINDOW_TITLE, colored_frame)
                else:
                    print(f"Incomplete frame received: dropped {len(raw_data)} bytes.")

            # 2. Process keyboard interrupt tokens immediately (1ms yield gives UI breathing room)
            if cv2.waitKey(1) & 0xFF == 27:  # Escape key
                break
                
        except Exception as err:
            print(f"Network drop detected. Re-establishing link... Error: {err}")
            time.sleep(0.5)

    cv2.destroyAllWindows()

if __name__ == "__main__":
    run_bios_stream_monitor()
