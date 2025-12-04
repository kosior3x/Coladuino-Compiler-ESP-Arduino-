# Coladuino-Compiler-ESP-Arduino-
 Compiler Arduino projekts on colab notes
 
PL: Kompilator Arduino dla ESP32/ESP8266 uruchamiany bezpośrednio w Google Colab. Idealny do mobilnego programowania mikrokontrolerów.
EN : Arduino Compiler for ESP32/ESP8266 running on Google Colab, optimized for mobile micro-controller programming.

# 🚀 Coladuino Compiler 🛠️: Mobilny Kompilator ESP32/ESP8266 na Google Colab

**Coladuino Compiler** to autorskie narzędzie, które pozwala programistom na **kompilację i przygotowanie projektów Arduino** dla mikrokontrolerów **ESP32** oraz **ESP8266** bezpośrednio w przeglądarce, dzięki mocy **Google Colaboratory**. Zaprojektowany z myślą o **mobilnym kodowaniu** i szybkich prototypach, Coladuino omija konieczność lokalnej instalacji złożonych narzędzi, takich jak Arduino IDE czy PlatformIO.

## ✨ Kluczowe Cechy Projektu
* **Pełna Kompilacja:** Umożliwia kompilację kodu Arduino (.ino) do pliku binarnego (.bin) gotowego do wgrania na ESP.
* **Mobilna Dostępność:** Działa bez zarzutu na tabletach i smartfonach dzięki środowisku Colab.
* **Łatwość Użycia:** Wystarczy wkleić kod i uruchomić komórki — instalacja narzędzi odbywa się automatycznie.
* **Dwujęzyczna Dokumentacja:** Pełna instrukcja w języku polskim i angielskim.

***

## 1. Instrukcja Użytkowania Notatnika (Colab) 💻
## 1. Notebook Usage Instructions (Colab) 💻

Ten projekt zawiera notatnik (`.ipynb`), który jest najlepiej uruchamiany w środowisku Google Colab.
*This project includes a notebook (`.ipynb`) which is best run in the Google Colab environment.*

| Krok (PL) | Opis (PL) | Step (EN) | Description (EN) |
| :--- | :--- | :--- | :--- |
| **1. Otwórz** | Otwórz plik `.ipynb` na GitHub i kliknij **"Open in Colab"**. | **1. Open** | Open the `.ipynb` file on GitHub and click **"Open in Colab"**. |
| **2. Zależności** | Uruchom pierwszą komórkę kodu, aby zainstalować wymagane biblioteki (narzędzia ESP). | **2. Dependencies** | Run the first code cell to install any required libraries (ESP toolchain). |
| **3. Wklej Kod** | Wklej swój kod Arduino w dedykowaną komórkę i uruchom ją. | **3. Insert Code** | Paste your Arduino code into the dedicated cell and run it. |
| **4. Kompiluj** | Wybierz z menu **Środowisko wykonawcze** > **Uruchom wszystko**, aby skompilować plik. | **4. Compile** | Go to the **Runtime** menu > select **Run all** to compile the file. |
| **5. Pobierz** | Skompilowany plik `.bin` zostanie automatycznie udostępniony do pobrania. | **5. Download** | The compiled `.bin` file will be automatically made available for download. |

***

## 2. Ważna Informacja Dotycząca Języka 🇵🇱 / 🇬🇧
## 2. Important Language Notice 🇵🇱 / 🇬🇧

### PL
Głównym językiem używanym w komentarzach kodu, wewnętrznej dokumentacji oraz niniejszej instrukcji jest **język polski**. Przepraszamy za wszelkie niedogodności. Staramy się, aby kluczowe funkcje i zmienne były nazwane w sposób zrozumiały międzynarodowo, ale pełna dokumentacja techniczna pozostaje w języku polskim.

### EN
The primary language used in code comments, internal documentation, and this manual is **Polish**. We apologize for any inconvenience. We strive to ensure that key functions and variables are named in an internationally understandable manner, but the full technical documentation remains in Polish.

***

## 3. Sugestie i Wkład (Contributions) ✨
## 3. Suggestions and Contributions ✨

### PL
**🚀 KAŻDA SUGESTIA dotycząca ulepszenia kompilatora, dodania wsparcia dla innych mikrokontrolerów lub poprawek jest mile widziana i zostanie natychmiast UWZGLĘDNIONA.** Zachęcamy do wnoszenia wkładu poprzez:
* Tworzenie **Issue** na GitHub w celu zgłoszenia błędów lub propozycji nowych funkcji.
* Wysyłanie **Pull Request** z własnymi zmianami i poprawkami kodu.

Dziękujemy za Twój czas i zaangażowanie!

### EN
**🚀 EVERY SUGGESTION regarding compiler improvement, adding support for other microcontrollers, or fixes is highly welcome and will be taken into IMMEDIATE CONSIDERATION.** We encourage contributions through:
* Creating an **Issue** on GitHub to report bugs or propose new features.
* Submitting a **Pull Request** with your own code changes and fixes.

Thank you for your time and involvement!
