#include "file_reader.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"

// ---------------------------------------------------------------------------------------------
// ---------------------------------------- OBRAZ DYSKU ----------------------------------------

// otwieranie dysku z pliku
struct disk_t* disk_open_from_file(const char* volume_file_name){
    // nazwa pliku null
    if(volume_file_name == NULL){
        errno = EFAULT;
        return NULL;
    }

    // odczyt pliku
    FILE *file = fopen(volume_file_name, "rb");
    if(file == NULL){
        errno = ENOENT;
        return NULL;
    }

    // uzyskanie rozmiaru pliku
    if(fseek(file, 0, SEEK_END) != 0){
        fclose(file);
        errno = ENOENT;
        return NULL;
    }
    long size = ftell(file);
    if(size < 0){
        fclose(file);
        errno = ENOENT;
        return NULL;
    }
    rewind(file);

    // alokacja struktury
    struct disk_t *disk = malloc(sizeof(struct disk_t));
    if(disk == NULL){
        fclose(file);
        errno = ENOMEM;
        return NULL;
    }

    // przypisanie danych do struktury
    disk->disk_file = file;
    disk->disk_size = (uint32_t) size;
    return disk;
}

// wczytanie okreslona liczbe sektorow z dysku do bufora, zaczynajac od konkretnego sektora
int disk_read(struct disk_t* pdisk, int32_t first_sector, void* buffer, int32_t sectors_to_read){
    // niepoprawne wskazniki
    if(pdisk == NULL || pdisk->disk_file == NULL || buffer == NULL){
        errno = EFAULT;
        return -1;
    }

    // niepoprawne argumenty
    if(first_sector < 0 || sectors_to_read < 0){
        errno = ERANGE;
        return -1;
    }

    uint64_t byte_offset = (uint64_t) first_sector * SECTOR_SIZE;
    uint64_t bytes_to_read = (uint64_t) sectors_to_read * SECTOR_SIZE;

    // przekroczenie wielkosci pliku
    if(byte_offset + bytes_to_read > pdisk->disk_size){
        errno = ERANGE;
        return -1;
    }

    // blad ustawienia pozycji
    if(fseek(pdisk->disk_file, (long) byte_offset, SEEK_SET) != 0){
        errno = EIO;
        return -1;
    }

    // sprawdzenie czy wczyta w calosci
    size_t read_blocks = fread(buffer, SECTOR_SIZE, sectors_to_read, pdisk->disk_file);
    if(read_blocks != (size_t) sectors_to_read){
        errno = EIO;
        return -1;
    }

    return sectors_to_read;
}

// zamkniecie dysku
int disk_close(struct disk_t* pdisk){
    // niepoprawne wskazniki
    if(pdisk == NULL || pdisk->disk_file == NULL){
        errno = EFAULT;
        return -1;
    }
    fclose(pdisk->disk_file);
    free(pdisk);
    return 0;
}

// -----------------------------------------------------------------------------------------
// ---------------------------------------- WOLUMIN ----------------------------------------

// otwarcie woluminu
struct volume_t* fat_open(struct disk_t* pdisk, uint32_t first_sector){
    if(pdisk == NULL){
        errno = EFAULT;
        return NULL;
    }

    // alokacja struktury woluminu
    struct volume_t *volume = malloc(sizeof(struct volume_t));
    if(volume == NULL){
        errno = ENOMEM;
        return NULL;
    }

    // inicjalizacja danych woluminu
    volume->disk = pdisk;
    volume->fat1_table = NULL;
    volume->fat2_table = NULL;

    // wczytanie boot_sector z first_sector
    if(disk_read(pdisk, (int32_t) first_sector, &volume->boot, 1) != 1){
        errno = EINVAL;
        free(volume);
        return NULL;
    }

    // sprawdzenie sygnatury boot_sector 55 AA
    uint8_t *boot_bytes = (uint8_t *) &volume->boot;
    if(boot_bytes[510] != 0x55 || boot_bytes[511] != 0xAA){
        errno = EINVAL;
        free(volume);
        return NULL;
    }

    // obliczenie rozmiaru FAT i tablice FAT
    uint16_t bytes_per_sector = volume->boot.bytes_per_sector;
    // uint8_t sectors_per_cluster = volume->boot.sectors_per_cluster;
    uint16_t reserved_sectors = volume->boot.reserved_sectors;
    uint8_t fat_count = volume->boot.fat_count;
    uint16_t sectors_per_fat = volume->boot.sectors_per_fat;

    uint32_t fat1_sector = first_sector + reserved_sectors;
    uint32_t fat_size_bytes = sectors_per_fat * bytes_per_sector;

    // tablica FAT1 alokacja
    volume->fat1_table = malloc(fat_size_bytes);
    if(volume->fat1_table == NULL){
        errno = ENOMEM;
        free(volume);
        return NULL;
    }

    // wczytanie danych do tablicy FAT1
    if(disk_read(pdisk, (int32_t) fat1_sector, volume->fat1_table, sectors_per_fat) != sectors_per_fat){
        errno = EINVAL;
        free(volume->fat1_table);
        free(volume);
        return NULL;
    }

    // tablica FAT2 kopia
    if(fat_count >= 2){

        // tablica FAT2 alokacja
        volume->fat2_table = malloc(fat_size_bytes);
        if(volume->fat2_table == NULL){
            errno = ENOMEM;
            free(volume->fat1_table);
            free(volume);
            return NULL;
        }

        // wczytanie danych do tablicy FAT2
        if(disk_read(pdisk, (int32_t) fat1_sector + sectors_per_fat, volume->fat2_table, sectors_per_fat) != sectors_per_fat){
            errno = EINVAL;
            free(volume->fat2_table);
            free(volume->fat1_table);
            free(volume);
            return NULL;
        }

        // opcjonalne sprawdzenie poprawnosci tabel
        if(memcmp(volume->fat1_table, volume->fat2_table, fat_size_bytes) != 0){
            errno = EINVAL;
            free(volume->fat2_table);
            free(volume->fat1_table);
            free(volume);
            return NULL;
        }
    }
    return volume;
}

// zamkniecie woluminu
int fat_close(struct volume_t* pvolume){
    // niepoprawny wskaznik
    if(pvolume == NULL){
        errno = EFAULT;
        return -1;
    }
    free(pvolume->fat2_table);
    free(pvolume->fat1_table);
    free(pvolume);
    return 0;
}

// ---------------------------------------------------------------------------------------
// ---------------------------------------- PLIKI ----------------------------------------

// otwieranie pliku z woluminu
struct file_t* file_open(struct volume_t* pvolume, const char* file_name){
    // nieprawidlowy wskaznik do woluminu
    if(pvolume == NULL){
        errno = EFAULT;
        return NULL;
    }

    // nieprawidlowy wskaznik do pliku
    if(file_name == NULL){
        errno = ENOENT;
        return NULL;
    }

    // offset katalogu glownego
    size_t root_dir_sectors = ((pvolume->boot.root_dir_capacity * 32) + (pvolume->boot.bytes_per_sector - 1)) / pvolume->boot.bytes_per_sector;
    size_t fat_size = pvolume->boot.sectors_per_fat;
    size_t reserved_sectors = pvolume->boot.reserved_sectors;
    size_t fat_count = pvolume->boot.fat_count;

    size_t first_root_dir_sector = reserved_sectors + fat_count * fat_size;
    size_t root_dir_bytes = root_dir_sectors * pvolume->boot.bytes_per_sector;

    // katalog glowny data
    uint8_t *root_dir_data = (uint8_t *) malloc(root_dir_bytes);
    if(root_dir_data == NULL){
        errno = ENOMEM;
        return NULL;
    }

    fseek(pvolume->disk->disk_file, (int32_t) first_root_dir_sector * pvolume->boot.bytes_per_sector, SEEK_SET);
    fread(root_dir_data, 1, root_dir_bytes, pvolume->disk->disk_file);

    // plik
    struct file_t *file = NULL;
    for(size_t i=0; i<pvolume->boot.root_dir_capacity; i++){
        uint8_t *entry = root_dir_data + i * 32;

        // koniec katalogu
        if(entry[0] == 0x00){
            break;
        }

        // usuniety plik
        if((uint8_t) entry[0] == 0xE5){
            continue;
        }

        // nazwa pliku
        char base_name[9] = {0};
        char extension[4] = {0};

        // nazwa bez spacji
        memcpy(base_name, entry, 8);
        for(int j = 7; j >= 0 && base_name[j] == ' '; j--) base_name[j] = '\0';

        // rozszerzenie bez spacji
        memcpy(extension, entry + 8, 3);
        for(int j = 2; j >= 0 && extension[j] == ' '; j--) extension[j] = '\0';

        // pelna nazwa pliku z rozszerzeniem
        char name[13] = {0};
        strcpy(name, base_name);
        if(extension[0] != '\0') {
            strcat(name, ".");
            strcat(name, extension);
        }

        // porownanie nazw (odrzucenie katalogu, tylko pliki)
        if(strcasecmp(name, file_name) == 0){
            uint8_t attribute = entry[11];
            if((attribute & ATTR_DIRECTORY) || (attribute & ATTR_VOLUME_ID)){
                free(root_dir_data);
                errno = EISDIR;
                return NULL;
            }

            uint16_t first_cluster = entry[26] | (entry[27] << 8);
            uint32_t file_size = entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24);

            file = (struct file_t*)malloc(sizeof(struct file_t));
            if(file == NULL){
                free(root_dir_data);
                errno = ENOMEM;
                return NULL;
            }

            file->volume = pvolume;
            file->size = file_size;
            file->first_cluster = first_cluster;
            file->current_cluster = first_cluster;
            file->current_position = 0;
            file->cluster_size = pvolume->boot.sectors_per_cluster * pvolume->boot.bytes_per_sector;

            break;
        }
    }
    free(root_dir_data);
    if(file == NULL){
        errno = ENOENT;
    }
    return file;
}

// zamykanie pliku
int file_close(struct file_t* stream){
    // nieprawidlowy wskaznik do pliku
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }
    free(stream);
    return 0;
}

// pomocnicza do nastepnego klastra
uint32_t next_cluster(struct volume_t *volume, uint32_t current_cluster){
    uint16_t *fat = (uint16_t *) volume->fat1_table;
    return fat[current_cluster];
}

// odczytanie nmemb elementow danych z pliku z voluminu
size_t file_read(void *ptr, size_t size, size_t nmemb, struct file_t *stream){
    // nieprawidlowe wskazniki
    if(ptr == NULL || stream == NULL || stream->volume == NULL){
        errno = EFAULT;
        return -1;
    }

    // koniec pliku lub brak danych do odczytania
    if(stream->current_position >= stream->size || size == 0 || nmemb == 0){
        return 0;
    }

    // ilosc bajtow do odczytania
    size_t remaining = stream->size - stream->current_position;
    size_t total_bytes = size * nmemb;
    if(total_bytes > remaining){ // by nie wyjsc poza plik
        total_bytes = remaining;
    }

    size_t bytes_read = 0; // przeczytanie bajty
    uint32_t position_in_cluster = stream->current_position % stream->cluster_size;  // pozycja w aktualnym klastrze

    // alokacja do bufora do odczytu z klastra
    uint8_t *cluster_buffer = malloc(stream->cluster_size);
    if(cluster_buffer == NULL){
        errno = ENXIO;
        return -1;
    }

    // przechodzenie przez klastry jeden po drugim
    while(bytes_read < total_bytes && stream->current_position < stream->size){
        uint32_t bytes_to_read = total_bytes - bytes_read;

        // wiecej danych do odczytu -> odczyt do konca klastra
        if(bytes_to_read > stream->cluster_size - position_in_cluster){
            bytes_to_read = stream->cluster_size - position_in_cluster;
        }

        // mapowanie numeru klastra na numer sektora
        uint32_t sectors_per_cluster = stream->cluster_size / SECTOR_SIZE;
        uint32_t first_data_sector = stream->volume->boot.reserved_sectors +
                                     stream->volume->boot.fat_count * stream->volume->boot.sectors_per_fat +
                                     ((stream->volume->boot.root_dir_capacity * 32) + (stream->volume->boot.bytes_per_sector - 1)) / stream->volume->boot.bytes_per_sector;
        uint32_t sector = first_data_sector + (stream->current_cluster - 2) * sectors_per_cluster;

        // odczytujemy dane z dysku
        if(disk_read(stream->volume->disk, (int32_t) sector, cluster_buffer, (int32_t) stream->cluster_size / SECTOR_SIZE) != (int) sectors_per_cluster){
            free(cluster_buffer);
            errno = ERANGE;
            return -1;
        }

        // skopiowanie danych do bufora
        uint8_t* ptr_byte = (uint8_t*) ptr;
        uint8_t* cluster_buffer_byte = (uint8_t*) cluster_buffer;
        memcpy(ptr_byte + bytes_read, cluster_buffer_byte + position_in_cluster, bytes_to_read);

        // zaktualizowanie pozycji
        bytes_read += bytes_to_read;
        stream->current_position += bytes_to_read;
        position_in_cluster += bytes_to_read;

        // jesli koniec klastra to kolejny
        if(position_in_cluster == stream->cluster_size){
            position_in_cluster = 0;
            stream->current_cluster = next_cluster(stream->volume, stream->current_cluster);
            if(stream->current_cluster >= FAT16_CLUSTER_END_MIN && stream->current_cluster <= FAT16_CLUSTER_END_MAX
               || stream->current_cluster == FAT16_CLUSTER_BAD
               || stream->current_cluster < FAT16_CLUSTER_MIN){
                break;
            }
        }
    }

    // zwolnienie bufora
    free(cluster_buffer);

    // liczba odczytanych elementow
    return bytes_read / size;
}

int32_t file_seek(struct file_t* stream, int32_t offset, int whence){
    // nieprawidlowy wskaznik do pliku
    if(stream == NULL){
        errno = EFAULT;
        return -1;
    }

    // aktualizacja pozycji
    int32_t new_position;
    if(whence == SEEK_SET){
        new_position = offset;
    }
    else if(whence == SEEK_CUR){
        new_position = (int32_t) stream->current_position + offset;
    }
    else if(whence == SEEK_END){
        new_position = (int32_t) stream->size + offset;
    }
    else{
        errno = EINVAL;
        return -1;
    }

    // sprawdzenie poprawnosci pozycji
    if(new_position < 0 || new_position > (int32_t) stream->size){
        errno = ENXIO;
        return -1;
    }

    // ustalenie nowego klastra
    uint32_t cluster = stream->first_cluster;
    uint32_t remaining = new_position;
    while(remaining >= stream->cluster_size){
        cluster = next_cluster(stream->volume, cluster);
        remaining -= stream->cluster_size;

        if(cluster >= FAT16_CLUSTER_END_MIN && cluster <= FAT16_CLUSTER_END_MAX || cluster == FAT16_CLUSTER_BAD || cluster < FAT16_CLUSTER_MIN){
            errno = ERANGE;
            return -1;
        }
    }

    stream->current_cluster = cluster;
    stream->current_position = (uint32_t) new_position;

    return new_position;
}

// ------------------------------------------------------------------------------------------
// ---------------------------------------- KATALOGI ----------------------------------------

// parsowanie wpisu
void parse_entry(uint8_t *entry, struct dir_entry_t *parsed_struct){
    // nazwa + rozszerzenie (8.3 format: 8 znakĂłw + kropka + 3 rozszerzenie)
    char base_name[9] = {0};
    char extension[4] = {0};
    memcpy(base_name, entry, 8); // nazwa bez spacji
    for(int j = 7; j >= 0 && base_name[j] == ' '; j--) base_name[j] = '\0';
    memcpy(extension, entry + 8, 3); // rozszerzenie bez spacji
    for(int j = 2; j >= 0 && extension[j] == ' '; j--) extension[j] = '\0';
    char name[13] = {0}; // pelna nazwa z rozszerzeniem
    strcpy(name, base_name);
    if(extension[0] != '\0'){
        strcat(name, ".");
        strcat(name, extension);
    }
    strcpy(parsed_struct->name, name);

    // rozmiar pliku (4 bajty, offset 28)
    parsed_struct->size = *(uint32_t *)(entry + 28);

    // pierwszy klaster (2 bajty, offset 26)
    parsed_struct->first_cluster = *(uint16_t*)(entry + 26);

    // atrybuty
    uint8_t attributes = entry[11];
    parsed_struct->is_archived = (attributes & 0x20) >> 5;
    parsed_struct->is_readonly = (attributes & 0x01);
    parsed_struct->is_system = (attributes & 0x04) >> 2;
    parsed_struct->is_volume = (attributes & 0x08) >> 3;
    parsed_struct->is_hidden = (attributes & 0x02) >> 1;
    parsed_struct->is_directory = (attributes & 0x10) >> 4;
}

// otwarcie katalogu glownego
struct dir_t* dir_open(struct volume_t* pvolume, const char* dir_path){
    // nieprawidlowy wskaznik
    if(pvolume == NULL){
        errno = EFAULT;
        return NULL;
    }

    // nieprawidlowa sciezka katalogu
    if(dir_path == NULL){
        errno = ENOENT;
        return NULL;
    }

    // tylko katalog glowny sprawdzam
    if(strcmp(dir_path, "\\") != 0){
        errno = ENOENT;
        return NULL;
    }

    // rozmiar root_directory
    size_t root_dir_sectors = ((pvolume->boot.root_dir_capacity * 32) + (pvolume->boot.bytes_per_sector - 1)) / pvolume->boot.bytes_per_sector;
    size_t first_root_dir_sector = pvolume->boot.reserved_sectors + (pvolume->boot.fat_count * pvolume->boot.sectors_per_fat);
    size_t root_dir_bytes = root_dir_sectors * pvolume->boot.bytes_per_sector;

    // cale dane root_directory
    uint8_t *dir_data = malloc(root_dir_bytes);
    if(dir_data == NULL){
        errno = ENOMEM;
        return NULL;
    }

    // wczytanie calego root_directory
    if(disk_read(pvolume->disk, (int32_t) first_root_dir_sector, dir_data, (int32_t) root_dir_sectors) != (int) root_dir_sectors){
        free(dir_data);
        errno = ERANGE;
        return NULL;
    }

    // lista wpisow z katalogu
    struct dir_entry_t *entries = malloc(pvolume->boot.root_dir_capacity * sizeof(struct dir_entry_t));
    if(entries == NULL){
        free(dir_data);
        errno = ENOMEM;
        return NULL;
    }

    // wczytanie wpisow do listy
    uint32_t entry_counter = 0;
    for(int i=0; i<pvolume->boot.root_dir_capacity; i++){
        uint8_t *entry = dir_data + i * 32;

        // koniec wpisow
        if(entry[0] == 0x00){
            break;
        }
        // uszkodzony
        if(entry[0] == 0xE5){
            continue;
        }

        // parsowanie wpisu
        struct dir_entry_t parsed_entry;
        parse_entry(entry, &parsed_entry);
        if(parsed_entry.is_volume){
            continue;
        }
        entries[entry_counter] = parsed_entry;

        entry_counter++;
    }

    free(dir_data);

    // katalog
    struct dir_t* dir = malloc(sizeof(struct dir_t));
    if(dir == NULL){
        free(entries);
        errno = ENOMEM;
        return NULL;
    }
    dir->volume = pvolume;
    dir->entries = entries;
    dir->entry_number = entry_counter;
    dir->current_entry = 0;
    return dir;
}

// zapisuje informacje o nastÄpnym poprawnym wpisie w katalogu pdir do struktury informacyjnej dir_entry_t
int dir_read(struct dir_t* pdir, struct dir_entry_t* pentry){
    // niepoprawne wskazniki
    if(pdir == NULL || pentry == NULL || pdir->volume == NULL || pdir->volume->disk == NULL || pdir->entries == NULL){
        errno = EFAULT;
        return -1;
    }

    // przechodzenie przez wszystkie wpisy katalogu
    while(pdir->current_entry < pdir->entry_number){
        struct dir_entry_t *entry = &pdir->entries[pdir->current_entry];
        pdir->current_entry++;

        // wpis pusty lub plik systemowy / wolumin
        if(entry->name[0] == '\0' || entry->is_volume){
            continue;
        }

        // kopiowanie danych do pentry
        *pentry = *entry;
        return 0; // wpis odczytany
    }
    return 1; // brak kolejnych wpisow
}

// zamykanie katalogu glownego
int dir_close(struct dir_t* pdir){
    // nieprawidlowy wskaznik
    if(pdir == NULL){
        errno = EFAULT;
        return -1;
    }
    free(pdir->entries);
    free(pdir);
    return 0;
}
