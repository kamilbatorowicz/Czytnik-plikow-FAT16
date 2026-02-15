#ifndef FAT16_2_FILE_READER_H
#define FAT16_2_FILE_READER_H

#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define SECTOR_SIZE 512
#define ENTRY_SIZE 32

// FAT16: znaczenia w tablicy FAT
#define FAT16_CLUSTER_FREE        0x0000  // wolny klaster
#define FAT16_CLUSTER_RESERVED    0x0001  // zarezerwowany/nie istnieje
#define FAT16_CLUSTER_MIN         0x0002  // poczatek lancucha pliku
#define FAT16_CLUSTER_MAX         0xFFF6  // maksymalna wartosc klastra w uzyciu
#define FAT16_CLUSTER_BAD         0xFFF7  // uszkodzony klaster
#define FAT16_CLUSTER_END_MIN     0xFFF8  // poczatek zakresu "koniec pliku"
#define FAT16_CLUSTER_END_MAX     0xFFFF  // ostatni mozliwy klaster (EOF)

struct boot_sector_t {
    uint8_t __jump_code[3];         // skok do programu ladujacego
    char oem_name[8];               // nazwa OEM "MSWIN4.1" lub "MSDOS5.0".

    // BPB - rekord ladujacy
    uint16_t bytes_per_sector;      // liczba bajtow w sektorze
    uint8_t sectors_per_cluster;    // liczba sektorow w klastrze
    uint16_t reserved_sectors;      // liczba zarezerwowanych sektorow
    uint8_t fat_count;              // liczba tabel alokacji plikow FAT
    uint16_t root_dir_capacity;     // liczba wpisow w katalogu glownym root_directory
    uint16_t logical_sectors16;     // calkowita liczba sektorow w woluminie
    uint8_t __reserved;             // typ nosnika (nieistotne wartosci)
    uint16_t sectors_per_fat;       // liczb sektorow na jedna tablice alokacji plikow FAT
    uint32_t __reserved2;           // liczba sektorow na sciezce + liczba sciezek w cylindrze
    uint32_t hidden_sectors;        // liczba ukrytych sektorow
    uint32_t logical_sectors32;     // calkowita liczba sektorow w woluminie (pole logical_sectors16 = 0)
    uint16_t __reserved3;           // oznaczenie numeru dysku + zarezerwowane (numer glowicy) (nieistotne wartosci)
    uint8_t __reserved4;            // sygnatura rozszerzonego rekordu ladujacego (nieistotne wartosci)
    uint32_t serial_number;         // unikalny numer seryjny woluminu

    char label[11];                 // nazwa woluminu wpis w katalogu glownym FAT (atrybut volume)
    char fsid[8];                   // identyfikator systemu plikow (FAT12, FAT16, FAT32, ...)
    uint8_t __boot_code[448];       // maszynowy kod ladowany przez BIOS (nieistotne wartosci)
    uint16_t magic;                 // 55 aa (znacznik konca)
} __attribute__(( packed ));

// obraz dysku (img/bin)
struct disk_t {
    FILE *disk_file;        // wskaznik do otwartego pliku dysku
    uint32_t disk_size;     // rozmiar pliku (w bajtach)
};

// wolumin
struct volume_t {
    struct disk_t *disk;           // wskaznik na dysk
    struct boot_sector_t boot;     // wczytany sektor startowy (boot sector)
    void *fat1_table;              // tablica fat1
    void *fat2_table;              // kopia tablicy fat1
};

// plik otwarty w systemie FAT
struct file_t {
    struct volume_t *volume;       // wskaznik na wolumin, z ktorego plik pochodzi
    uint32_t size;                 // rozmiar pliku w bajtach
    uint32_t current_position;     // aktualna pozycja w pliku
    uint32_t first_cluster;        // pierwszy klaster pliku (numer)
    uint32_t current_cluster;      // aktualny klaster (czytany)
    uint32_t cluster_size;         // rozmiar klastra
};

// katalog / folder
struct dir_entry_t {
    char name[13];            // nazwa pliku 8-nazwa, 1-kropka, 3-rozszerzenie, 1-'\0'
    uint32_t size;            // rozmiar pliku lub katalogu w bajtach
    uint16_t first_cluster;   // pierwszy klaster
    uint8_t is_archived;      // 1 - zarchiwizowany, 0 - nie
    uint8_t is_readonly;      // 1 - tylko do odczytu, 0 - nie
    uint8_t is_system;        // 1 - systemowy, 0 - nie
    uint8_t is_volume;        // 1 - wolumin, 0 - nie
    uint8_t is_hidden;        // 1 - ukryty, 0 - nie
    uint8_t is_directory;     // 1 - katalog, 0 - plik
};

struct dir_t{
    struct volume_t *volume;      // wskaznik na wolumin, w ktorym jest folder
    uint32_t entry_number;        // liczba wpisow w katalogu
    uint32_t current_entry;       // aktualny indeks wpisu
    struct dir_entry_t *entries;  // wpisy katalogowe
};

// atrybuty FAT
enum fat_attributes_t {
    ATTR_READ_ONLY = 0x01,
    ATTR_HIDDEN = 0x02,
    ATTR_SYSTEM = 0x04,
    ATTR_VOLUME_ID = 0x08,
    ATTR_DIRECTORY = 0x10,
    ATTR_ARCHIVE = 0x20,
    ATTR_LFN = ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID // 0x0F
} __attribute__(( packed ));

// ---------- FUNKCJE ----------

// urzadzenie blokowe / sektor
struct disk_t* disk_open_from_file(const char* volume_file_name);
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read);
int disk_close(struct disk_t* pdisk);

// wolumin FAT16
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector);
int fat_close(struct volume_t* pvolume);

// pliki w systemie FAT16
struct file_t* file_open(struct volume_t* pvolume, const char* file_name);
int file_close(struct file_t* stream);
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream);
int32_t file_seek(struct file_t* stream, int32_t offset, int whence);

// katalogi w systemie FAT16
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path);
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry);
int dir_close(struct dir_t* pdir);

#endif //FAT16_2_FILE_READER_H