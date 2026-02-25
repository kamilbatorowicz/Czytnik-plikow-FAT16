# Czytnik Systemu Plików FAT16 💾

![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c&logoColor=white)
![File System](https://img.shields.io/badge/FS-FAT16-orange?style=for-the-badge)
![Status](https://img.shields.io/badge/Status-Completed-success?style=for-the-badge)

  
## 📋 Opis projektu

Projekt polega na implementacji parsera obrazów dyskowych zapisanych w formacie **FAT16**. Jest to niskopoziomowe narzędzie pozwalające na interakcję z systemem plików bez montowania go w systemie operacyjnym. Program realizuje pełną ścieżkę odczytu: od surowych bajtów urządzenia blokowego, przez interpretację struktur sterujących (Boot Sector), aż po nawigację w drzewie katalogów i odczyt konkretnych plików.

---

## 🏗️ Struktura Projektu (Zgodnie z wymaganiami)

### 1. Opis problemu
Głównym wyzwaniem jest poprawna interpretacja specyfikacji FAT16, w tym obsługa formatu **Little Endian**, parsowanie wpisów katalogowych (SFN - Short File Names) oraz poprawne przechodzenie po łańcuchach klastrów w tablicy FAT.

### 2. Obsługa urządzenia blokowego
Implementacja warstwy abstrakcji dla "urządzenia", które w tym przypadku jest plikiem binarnym (obrazem dysku `.img` / `.raw`).
* Bezpieczne otwieranie i zamykanie strumienia.
* Mechanizm precyzyjnego przesuwania wskaźnika odczytu (`fseek`) do konkretnych sektorów.

### 3. Zarządzanie Woluminem FAT16
* **Boot Sector Parsing**: Odczyt parametrów BPB (BIOS Parameter Block), takich jak liczba sektorów na klaster, rozmiar zarezerwowanego obszaru czy liczba tablic FAT.
* Weryfikacja sygnatur systemu plików w celu zapewnienia spójności danych.

### 4. Nawigacja i odczyt danych
* **Katalogi**: Listowanie zawartości katalogu głównego (Root Directory) oraz podkatalogów.
* **Pliki**: Mechanizm wyszukiwania plików po nazwie i odczytywanie ich zawartości poprzez śledzenie powiązań w tablicy FAT (File Allocation Table).

### 5. Podsumowanie
Projekt dostarcza solidne API do analizy obrazów dysków. Pozwala na wyodrębnianie danych z uszkodzonych obrazów lub naukę wewnętrznej struktury systemów plików stosowanych w systemach wbudowanych (Embedded) oraz starszych systemach operacyjnych (DOS).

---

## 🛠️ Technologie

* **Język:** C (Standard C99)
* **Format danych:** FAT16 (Little Endian)
* **Narzędzia:** Hex Editor (do weryfikacji danych), Makefile, GCC

---

## 🚀 Szybki start

1. Sklonuj repozytorium:
   ```bash
   git clone [https://github.com/TwojUser/Czytnik-FAT16.git](https://github.com/TwojUser/Czytnik-FAT16.git)
