import json
import time
import threading
import serial
import websocket

class RobotInterface:
    """Uniwersalny interfejs do komunikacji z robotem ESP32."""

    def __init__(self, mode='ws', port='192.168.4.1', baudrate=115200):
        self.mode = mode
        self.port = port
        self.baudrate = baudrate
        self.connection = None
        self.running = False
        self.telemetry = {}
        self.on_telemetry_cb = None

    def connect(self):
        if self.mode == 'ws':
            url = f"ws://{self.port}:81"
            self.connection = websocket.create_connection(url, timeout=5)
        else:
            self.connection = serial.Serial(self.port, self.baudrate, timeout=1)

        self.running = True
        self.thread = threading.Thread(target=self._read_loop, daemon=True)
        self.thread.start()
        print(f"Connected to robot via {self.mode.upper()}")

    def _read_loop(self):
        while self.running:
            try:
                if self.mode == 'ws':
                    data = self.connection.recv()
                else:
                    data = self.connection.readline().decode('utf-8').strip()

                if data:
                    msg = json.loads(data)
                    self.telemetry = msg
                    if self.on_telemetry_cb:
                        self.on_telemetry_cb(msg)
            except Exception as e:
                if self.running:
                    print(f"Read error: {e}")
                break

    def send_command(self, action, speed_l=100, speed_r=100):
        cmd = {
            "type": "command",
            "action": action,
            "speed_left": speed_l,
            "speed_right": speed_r
        }
        self._send(cmd)

    def _send(self, data):
        msg = json.dumps(data)
        if self.mode == 'ws':
            self.connection.send(msg)
        else:
            self.connection.write((msg + '\n').encode('utf-8'))

    def close(self):
        self.running = False
        if self.connection:
            self.connection.close()
        print("Connection closed")

if __name__ == "__main__":
    # Testowy przykład użycia
    def handle_telemetry(data):
        print(f"Sensors: {data}")

    # Ustaw mode='serial' i odpowiedni port (np. 'COM3' lub '/dev/ttyUSB0') dla kabla
    robot = RobotInterface(mode='ws', port='192.168.4.1')
    try:
        robot.connect()
        robot.on_telemetry_cb = handle_telemetry
        while True:
            robot.send_command("FORWARD", 100, 100)
            time.sleep(2)
            robot.send_command("STOP")
            time.sleep(1)
    except KeyboardInterrupt:
        robot.close()
