////////////////////////////////////////////////////////////////////////
// COMP1521 23T1 --- Assignment 2: `rain', a simple file archiver
// <https://www.cse.unsw.edu.au/~cs1521/23T1/assignments/ass2/index.html>
//
// Written by Gabriel Esquivel (z5358503) on 23/04/2023.
// Program Description: A file archiver for the drop format. The drop format
// is made up of one or more droplets; where a droplet records one file system
// object.
//
// 2021-11-08   v1.1    Team COMP1521 <cs1521 at cse.unsw.edu.au>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include "rain.h"

// ADD ANY extra #defines HERE
#define MAX_PATHNAME_SIZE 144
#define PERMISSIONS_LENGTH 10
#define FOUR_DIGITS 9999
#define THREE_DIGITS 999
#define TWO_DIGITS 99

// ADD YOUR FUNCTION PROTOTYPES HERE
int two_byte_little_endian_to_int(char byte1, char byte2);
void int_to_two_byte_little_endian(int n, unsigned char* bytes);
long six_byte_little_endian_to_int(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4, unsigned char byte5, unsigned char byte6);
void int_to_six_byte_little_endian(int n, unsigned char* bytes); 
int read_droplet_format(FILE *f);
char* read_permissions(FILE *f);
int read_pathname_length(FILE *f);
void read_pathname(FILE *f, char* str, int pathname_length);
int read_content_length(FILE *f);
void read_content(FILE *f, char* str, int content_length);
int* read_char_update_hash(FILE *f, uint8_t current_hash);
void set_file_permissions(const char *pathname, const char *permissions);
char* get_file_permissions(char* pathname);

// print the files & directories stored in drop_pathname (subset 0)
//
// if long_listing is non-zero then file/directory permissions, formats & sizes are also printed (subset 0)

void list_drop(char *drop_pathname, int long_listing) {

    FILE *file = fopen(drop_pathname, "rb");
    if (file == NULL) {
        printf("Failed to open file: %s\n", drop_pathname);
        return;
    }
    
    while (fgetc(file) != EOF) {
        
        // Skip past magic number
        fseek(file, 0, SEEK_CUR);

        // Read droplet format
        int droplet_format = read_droplet_format(file);

        // Read permissions
        char* permissions = read_permissions(file);

        // Read length of pathname       
        int pathname_length = read_pathname_length(file);

        // Read pathname
        char pathname[pathname_length];
        read_pathname(file, pathname, pathname_length);

        // Read content length
        int content_length = read_content_length(file);

        // Skip content and hash
        fseek(file, content_length + 1, SEEK_CUR);

        // If long listing, print extra information
        if (long_listing) {
            // Print permissions
            for (int i = 0; i < PERMISSIONS_LENGTH; i++) {
                printf("%c", permissions[i]);
            }

            // Print droplet format
            printf("  %d", droplet_format);
            if (content_length > FOUR_DIGITS) {
                printf("  ");
            } else if (content_length > THREE_DIGITS) {
                printf("   ");
            } else if (content_length > TWO_DIGITS) {
                printf("    ");
            } else {
                printf("     ");
            }

            // Print file size
            printf("%d  ", content_length);

            // Print file pathname
            for (int i = 0; i < pathname_length; i++) {
                printf("%c", pathname[i]);
            }
            printf("\n");
            
        } else {
            // Print file pathname
            for (int i = 0; i < pathname_length; i++) {
                printf("%c", pathname[i]);
            }
            printf("\n");
        }
    }
    fclose(file);
}

// check the files & directories stored in drop_pathname (subset 1)
//
// prints the files & directories stored in drop_pathname with a message
// either, indicating the hash byte is correct, or
// indicating the hash byte is incorrect, what the incorrect value is and the correct value would be

void check_drop(char *drop_pathname) {

    FILE *file = fopen(drop_pathname, "rb");
    if (file == NULL) {
        printf("Failed to open file: %s\n", drop_pathname);
        return;
    }

    while (fgetc(file) != EOF) {

        fseek(file, -1, SEEK_CUR);
        uint8_t real_hash = 0;

        // Read magic number and update hash
        int *result = read_char_update_hash(file, real_hash); // Byte 0
        int magic_number = result[0];
        char hex[3];
        sprintf(hex, "%02x", magic_number);
        real_hash = result[1];

        // Check if magic number is valid
        if (magic_number != 'c') {
            fprintf( stderr, "error: incorrect first droplet byte: 0x%02x should be 0x%02x\n", magic_number, 'c');
            fclose(file);
            return;
        }

        // Hash droplet format and permissions
        for (int i = 0; i < 11; i++) {
            result = read_char_update_hash(file, real_hash);
            real_hash = result[1];
        }

        // Read and hash pathname length
        result = read_char_update_hash(file, real_hash);
        char byte1 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte2 = result[0];
        real_hash = result[1];
        int pathname_length = two_byte_little_endian_to_int(byte1, byte2);

        // Read and hash pathname
        char pathname[pathname_length];
        for (int i = 0; i < pathname_length; i++) {
            result = read_char_update_hash(file, real_hash);
            real_hash = result[1];
            int character = result[0];
            pathname[i] = (char)character;
        }

        // Read and hash content length
        result = read_char_update_hash(file, real_hash);
        char byte3 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte4 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte5 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte6 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte7 = result[0];
        real_hash = result[1];
        result = read_char_update_hash(file, real_hash);
        char byte8 = result[0];
        real_hash = result[1];
        int content_length = six_byte_little_endian_to_int(byte3, byte4, byte5, byte6, byte7, byte8);

        // Hash content
        for (int i = 0; i < content_length; i++) {
            result = read_char_update_hash(file, real_hash);
            real_hash = result[1];
        }

        // Read hash
        int read_hash = fgetc(file);

        // Compare hash
        if (real_hash == read_hash) {
            for (int i = 0; i < pathname_length; i++) {
                printf("%c", pathname[i]);
            }
            printf(" - correct hash\n");

        } else {
            for (int i = 0; i < pathname_length; i++) {
                printf("%c", pathname[i]);
            }
            printf(" - incorrect hash 0x%02x should be 0x%02x\n", real_hash, read_hash);
        }
    }
    fclose(file);
}

// extract the files/directories stored in drop_pathname (subset 2 & 3)

void extract_drop(char *drop_pathname) {

    FILE *file = fopen(drop_pathname, "r+");
    if (file == NULL) {
        printf("Failed to open file: %s\n", drop_pathname);
        return;
    }

    while (fgetc(file) != EOF) {

        fseek(file, 1, SEEK_CUR); 

        // Read permissions
        char* permissions = read_permissions(file);

        // Read length of pathname       
        int pathname_length = read_pathname_length(file);

        // Read pathname
        char pathname_str[MAX_PATHNAME_SIZE];
        char pathname_str2[MAX_PATHNAME_SIZE];
        strcpy(pathname_str2, pathname_str);
        read_pathname(file, pathname_str, pathname_length);
        pathname_str[pathname_length] = '\0';

        // Read length of content
        int content_length = read_content_length(file);

        // Read content
        char content[content_length + 2];
        read_content(file, content, content_length);

        // Skip hash
        fseek(file, 1, SEEK_CUR);

        // Print pathname of file to extract
        printf("Extracting: ");
        for (int i = 0; i < pathname_length; i++) {
            printf("%c", pathname_str[i]);
        }
        printf("\n");

        // Open file for writing
        FILE *outfile = fopen(pathname_str, "w+");
        if (!outfile) {
            printf("Failed to open output file: %s\n", pathname_str);
            continue;
        }
        
        // Write contents to file
        for (int j = 0; j < content_length; j++) {
            fprintf(outfile, "%c", content[j]);
        }

        // Set permissions
        set_file_permissions(pathname_str, permissions);
        fclose(outfile);
    }
    fclose(file);
}

// create drop_pathname containing the files or directories specified in pathnames (subset 3)
//
// if append is zero drop_pathname should be over-written if it exists
// if append is non-zero droplets should be instead appended to drop_pathname if it exists
//
// format specifies the droplet format to use, it must be one DROPLET_FMT_6,DROPLET_FMT_7 or DROPLET_FMT_8

void create_drop(char *drop_pathname, int append, int format, int n_pathnames, char *pathnames[n_pathnames]) {

    FILE *drop = fopen(drop_pathname, "ab");
    if (drop == NULL) {
        perror("Error creating drop");
        exit(EXIT_FAILURE);
    }
    int pathname_counter = 0;

    while (pathname_counter < n_pathnames) {

        // Start hash
        uint8_t hash = 0;

        // Get pathname
        char* pathname = pathnames[pathname_counter];
        pathname_counter++;
        
        // Print pathname and function
        printf("Adding: %s\n", pathname);

        // Put magic number in drop
        fputc('c', drop);
        hash = droplet_hash(hash, 'c');

        // Format
        fputc(format, drop);
        hash = droplet_hash(hash, (int)format);
        
        // Open droplet
        FILE *tmp = fopen(pathname, "rb");
        if (tmp == NULL) {
            perror("Error opening droplet");
            exit(EXIT_FAILURE);
        }

        // Permissions
        char* permissions = get_file_permissions(pathname);
        for(int k = 0; k < PERMISSIONS_LENGTH; k++) {
            fputc(permissions[k], drop);
            hash = droplet_hash(hash, (int)permissions[k]);
        }
        
        // Get pathname length as two byte little endian
        unsigned char pathname_length[2];
        int_to_two_byte_little_endian(strlen(pathname), pathname_length);

        // Add pathname length to drop and hash
        fputc(pathname_length[0], drop);
        hash = droplet_hash(hash, (int)pathname_length[0]);
        fputc(pathname_length[1], drop);
        hash = droplet_hash(hash, (int)pathname_length[1]);

        // Pathname
        for(int l = 0; l < strlen(pathname); l++) {
            fputc(pathname[l], drop);
            hash = droplet_hash(hash, (int)pathname[l]);
        }
        
        // Content Length
        fseek(tmp, 0, SEEK_END);
        int content_length = ftell(tmp);
        
        unsigned char content_length_bytes[6];
        int_to_six_byte_little_endian(content_length, content_length_bytes);
        for (int j = 0; j < 6; j++) {
            fputc(content_length_bytes[j], drop);
            hash = droplet_hash(hash, (int)content_length_bytes[j]);
        }

        // Content
        fseek(tmp, 0, SEEK_SET);
        char buffer[1024];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), tmp)) > 0) {
            fwrite(buffer, 1, bytes_read, drop);
            for (size_t i = 0; i < bytes_read; i++) {
                hash = droplet_hash(hash, (int)buffer[i]);
            }
        }
        fputc((char)hash, drop);
        fclose(tmp);    
    }
    fclose(drop);
}

// ADD YOUR EXTRA FUNCTIONS HERE

// Given a two byte little endian, return as int

int two_byte_little_endian_to_int(char byte1, char byte2) {
    int result = (byte2 << 8) | byte1;
    return result;
}

// Given an int, update the array of chars provided to be the two byte little endian representation

void int_to_two_byte_little_endian(int n, unsigned char *bytes) {
    bytes[0] = n & 0xFF;
    bytes[1] = (n >> 8) & 0xFF;
} 

// Given a six byte little endian, return as int

long six_byte_little_endian_to_int(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4, unsigned char byte5, unsigned char byte6) {
    long low_bytes = ((byte3 & 0xFF) << 16) | ((byte2 & 0xFF) << 8) | (byte1 & 0xFF);
    long high_bytes = ((byte6 & 0xFF) << 16) | ((byte5 & 0xFF) << 8) | (byte4 & 0xFF);
    if (high_bytes < 0) {
        high_bytes += (1 << 24);
    }
    return (high_bytes << 24) | low_bytes;
}

// Given an int, update the array of chars provided to be the six byte little endian representation

void int_to_six_byte_little_endian(int n, unsigned char *bytes) {
    bytes[0] = n & 0xFF;
    bytes[1] = (n >> 8) & 0xFF;
    bytes[2] = (n >> 16) & 0xFF;
    bytes[3] = (n >> 24) & 0xFF;
    bytes[4] = 0;
    bytes[5] = 0;
}

// Read and return the droplet format given the file

int read_droplet_format(FILE *f) {
    int result = 0;
    int byte = fgetc(f);
    result |= byte;

    return result/7;
}

// Read and return permissions given the file

char* read_permissions(FILE *f) {
    static char str[10];
    for (int index = 0; index < 10; index++) {
        str[index] = fgetc(f);
    }
    return str;
}

// Read and return pathname length given the file

int read_pathname_length(FILE *f) {
    char byte1 = fgetc(f);
    char byte2 = fgetc(f);
    return two_byte_little_endian_to_int(byte1, byte2);
}

// Read pathname and update pathname given the file and pathname length

void read_pathname(FILE *f, char* str, int pathname_length) {
    char c;
    for (int index = 0; index < pathname_length; index++) {
        c = fgetc(f);
        str[index] = c;
    }
}

// Read and return content length

int read_content_length(FILE *f) {
    char byte3 = fgetc(f);
    char byte4 = fgetc(f);
    char byte5 = fgetc(f);
    char byte6 = fgetc(f);
    char byte7 = fgetc(f);
    char byte8 = fgetc(f);
    return six_byte_little_endian_to_int(byte3, byte4, byte5, byte6, byte7, byte8);
}

// Given the pathname and length, read the content and update given string

void read_content(FILE *f, char* str, int content_length) {
    char c;
    for (int index = 0; index < content_length; index++) {
        c = fgetc(f);
        str[index] = c;;
    }
}

// Read a character and update the hash. Return an array of the character and the updated hash.

int* read_char_update_hash(FILE *f, uint8_t current_hash) {
    char c = fgetc(f);
    uint8_t byte_value = c;
    static int result[2] = {0};
    result[0] = c;
    result[1] = droplet_hash(current_hash, byte_value);
    return result;
}

// Given the pathname and desired permissions, set these permissions for the file.

void set_file_permissions(const char *pathname, const char *permissions) {
    char permissions_in_binary[PERMISSIONS_LENGTH + 1] = {0};
    for (int i = 0; i < PERMISSIONS_LENGTH; i++) {
        if (permissions[i] == '-') {
            strcat(permissions_in_binary, "0");
        } else {
            strcat(permissions_in_binary, "1");
        }
    }
    mode_t droplet_mode = strtol(permissions_in_binary, NULL, 2);
    if (chmod(pathname, droplet_mode) != 0) {
        perror(pathname);
        exit(1);
    }
}

// Return file permissions of the given pathname.

char* get_file_permissions(char* pathname) {
    struct stat file_stat;
    char* permissions = (char*)malloc(sizeof(char) * 11);

    if (stat(pathname, &file_stat) != 0) {
        perror("Error getting file permissions");
        exit(EXIT_FAILURE);
    }
    
    // Convert permissions to string representation
    permissions[0] = (S_ISDIR(file_stat.st_mode)) ? 'd' : '-';
    permissions[1] = (file_stat.st_mode & S_IRUSR) ? 'r' : '-';
    permissions[2] = (file_stat.st_mode & S_IWUSR) ? 'w' : '-';
    permissions[3] = (file_stat.st_mode & S_IXUSR) ? 'x' : '-';
    permissions[4] = (file_stat.st_mode & S_IRGRP) ? 'r' : '-';
    permissions[5] = (file_stat.st_mode & S_IWGRP) ? 'w' : '-';
    permissions[6] = (file_stat.st_mode & S_IXGRP) ? 'x' : '-';
    permissions[7] = (file_stat.st_mode & S_IROTH) ? 'r' : '-';
    permissions[8] = (file_stat.st_mode & S_IWOTH) ? 'w' : '-';
    permissions[9] = (file_stat.st_mode & S_IXOTH) ? 'x' : '-';

    permissions[10] = '\0';

    return permissions;
}