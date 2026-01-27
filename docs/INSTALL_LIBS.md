# Instalacja bibliotek i budowa lokalnie (krótko)

W repo używane biblioteki:
- ArduinoJson
- WebSockets

Polecenia (lokalnie, wymagany arduino-cli):
1. Zainstaluj arduino-cli (instrukcje: https://arduino.github.io/arduino-cli/latest/installation/)
2. Zainicjuj config i dodaj adres URL dla ESP32:
   arduino-cli config init
   arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core update-index
3. Zainstaluj rdzeń ESP32 i biblioteki:
   arduino-cli core install esp32:esp32
   arduino-cli lib install "ArduinoJson@6.21.3"
   arduino-cli lib install "WebSockets"

4. Kompilacja (z katalogu repo):
   arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --export-binaries sketches/serial_binary --build-path build/serial_binary
   arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --export-binaries sketches/wifi_ws --build-path build/wifi_ws

Wgrywanie:
- Możesz użyć `arduino-cli upload --fqbn esp32:esp32:esp32doit-devkit-v1 -p /dev/XXX` (po podłączeniu i podaniu odpowiedniego portu), albo esptool jeśli wolisz.

Uwagi:
- Jeśli chcesz dodać kolejne biblioteki, dodaj komendę `arduino-cli lib install "NazwaBiblioteki"` do workflow w sekcji `Install esp32 core and libraries`.
- Na telefonie najprościej: edytuj/stwórz pliki przez GitHub → Add file → Create new file, wklej zawartość i commit.
