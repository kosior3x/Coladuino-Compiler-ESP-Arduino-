# Sterowanie robotem za pomocą Python

W katalogu `scripts/python/` znajdują się narzędzia do zdalnego sterowania robotem.

## Wymagania

Zainstaluj wymagane biblioteki:
```bash
pip install pyserial websocket-client keyboard
```

## Opis plików

1.  **`robot_interface.py`**: Główna klasa obsługująca komunikację. Można jej używać w trybie WebSocket (`mode='ws'`) lub Serial (`mode='serial'`).
2.  **`keyboard_controller.py`**: Skrypt pozwalający na sterowanie robotem za pomocą klawiatury (WSAD).

## Jak uruchomić?

### 1. Sterowanie przez WiFi (WebSocket)
Upewnij się, że Twój komputer jest w tej samej sieci co robot (domyślnie AP: `SWARM_ROBOT`).

```bash
python scripts/python/keyboard_controller.py
```

### 2. Sterowanie przez kabel (Serial)
Podłącz robota przez USB. W pliku `keyboard_controller.py` zmień parametry inicjalizacji:
```python
robot = RobotInterface(mode='serial', port='COM3') # Podaj swój port
```

## Nowe funkcje stabilności
Wprowadzono mechanizm **Watchdog (Heartbeat)**. Jeśli robot nie otrzyma żadnej komendy przez 2 sekundy, automatycznie zatrzyma silniki. Zapewnia to bezpieczeństwo w przypadku utraty połączenia WiFi.
