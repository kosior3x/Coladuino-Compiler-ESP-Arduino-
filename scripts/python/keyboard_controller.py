import sys
import time
from robot_interface import RobotInterface

try:
    import keyboard # Wymaga pip install keyboard
except ImportError:
    print("BŁĄD: Zainstaluj bibliotekę keyboard: pip install keyboard")
    sys.exit(1)

def run_keyboard_control():
    print("--- STEROWANIE ROBOTEM (WSAD) ---")
    print("W - Przód, S - Tył, A - Lewo, D - Prawo, Spacja - STOP")
    print("Esc - Wyjście")

    # Zmień mode='serial' i port='/dev/ttyUSB0' dla kabla
    robot = RobotInterface(mode='ws', port='192.168.4.1')

    try:
        robot.connect()
    except Exception as e:
        print(f"Nie udało się połączyć: {e}")
        return

    speed = 120

    try:
        while True:
            if keyboard.is_pressed('w'):
                robot.send_command("FORWARD", speed, speed)
                print("Jazda: PRZÓD", end='\r')
            elif keyboard.is_pressed('s'):
                robot.send_command("BACKWARD", -speed, -speed)
                print("Jazda: TYŁ  ", end='\r')
            elif keyboard.is_pressed('a'):
                robot.send_command("TURN_LEFT", -speed, speed)
                print("Jazda: LEWO ", end='\r')
            elif keyboard.is_pressed('d'):
                robot.send_command("TURN_RIGHT", speed, -speed)
                print("Jazda: PRAWO", end='\r')
            elif keyboard.is_pressed('space'):
                robot.send_command("STOP")
                print("Akcja: STOP ", end='\r')

            if keyboard.is_pressed('esc'):
                break

            time.sleep(0.1)
    finally:
        robot.send_command("STOP")
        robot.close()
        print("\nZakończono sterowanie.")

if __name__ == "__main__":
    run_keyboard_control()
