#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"

int main() {
    const char *disk_name = "0001size_fat16_volume.img"; 

    // Otwieranie "dysku" (pliku obrazu)
    struct disk_t *disk = disk_open_from_file(disk_name);
    if (disk == NULL) {
        perror("Blad disk_open_from_file");
        return 1;
    }

    // Otwieranie woluminu FAT od sektora 0
    struct volume_t *volume = fat_open(disk, 0);
    if (volume == NULL) {
        perror("Blad fat_open");
        disk_close(disk);
        return 2;
    }

    // Wyświetlenie podstawowych informacji z Boot Sectora
    printf("Dysk: %s\n", disk_name);
    printf("System plikow: %.8s\n", volume->boot.fsid);
    printf("Bajtów na sektor: %u\n", volume->boot.bytes_per_sector);

    // Otwieranie katalogu głównego
    struct dir_t *dir = dir_open(volume, "\\");
    if (dir == NULL) {
        perror("Blad dir_open");
    } else {
        printf("\nZawartosc katalogu glownego:\n");
        struct dir_entry_t entry;
        // Czytanie kolejnych wpisów
        while (dir_read(dir, &entry) == 0) {
            printf("[%s] %-12s rozmiar: %u bajtow\n", 
                   entry.is_directory ? "DIR " : "FILE", 
                   entry.name, 
                   entry.size);
        }
        dir_close(dir);
    }

    // Sprzątanie
    fat_close(volume);
    disk_close(disk);

    return 0;
}