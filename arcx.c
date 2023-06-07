#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_FILENAME_LENGTH 256

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    long offset;
    long size;
} FileEntry;

int pack(const char* archiveFilename, const char* directory) {
    // Open the source directory
    DIR* dir = opendir(directory);
    if (dir == NULL) {
        printf("Failed to open source directory '%s'.\n", directory);
        perror("Error");
        return 1;
    }

    // Create a new archive file
    FILE* archiveFile = fopen(archiveFilename, "wb");
    if (archiveFile == NULL) {
        printf("Failed to create archive file '%s'.\n", archiveFilename);
        perror("Error");
        closedir(dir);
        return 1;
    }

    // Read files from the source directory
    struct dirent* entry;
    int fileCount = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Skip directories
        if (entry->d_type == DT_DIR) {
            continue;
        }

        // Construct the file path
        char filePath[MAX_FILENAME_LENGTH];
        snprintf(filePath, sizeof(filePath), "%s/%s", directory, entry->d_name);

        // Open the source file in binary mode
        FILE* srcFile = fopen(filePath, "rb");
        if (srcFile == NULL) {
            printf("Failed to open source file '%s'.\n", filePath);
            perror("Error");
            continue;
        }

        // Get the size of the source file
        fseek(srcFile, 0, SEEK_END);
        long size = ftell(srcFile);

        // Create a new file entry
        FileEntry fileEntry;
        strncpy(fileEntry.filename, entry->d_name, MAX_FILENAME_LENGTH);
        fileEntry.offset = ftell(archiveFile);
        fileEntry.size = size;

        // Write the file entry to the archive file
        if (fwrite(&fileEntry, sizeof(FileEntry), 1, archiveFile) != 1) {
            printf("Failed to write file entry to archive file '%s'.\n", archiveFilename);
            perror("Error");
            fclose(srcFile);
            closedir(dir);
            fclose(archiveFile);
            return 1;
        }

        // Reset the file position indicator to the beginning of the source file
        rewind(srcFile);

        // Copy the contents of the source file to the archive file
        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
            if (fwrite(buffer, 1, bytesRead, archiveFile) != bytesRead) {
                printf("Failed to write file data to archive file '%s'.\n", archiveFilename);
                perror("Error");
                fclose(srcFile);
                closedir(dir);
                fclose(archiveFile);
                return 1;
            }
        }

        // Close the source file
        fclose(srcFile);

        fileCount++;
    }

    // Close the archive file
    fclose(archiveFile);

    // Close the source directory
    closedir(dir);

    printf("%d file(s) packed into archive '%s'.\n", fileCount, archiveFilename);

    return 0;
}

int unpack(const char* archiveFilename, const char* directory) {
    // Open the archive file
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Failed to open archive file '%s'.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Create the destination directory if it doesn't exist
    if (mkdir(directory, 0777) != 0 && errno != EEXIST) {
        printf("Failed to create destination directory '%s'.\n", directory);
        perror("Error");
        fclose(archiveFile);
        return 1;
    }

    // Read the file entries from the archive file
    FileEntry fileEntry;
    int fileCount = 0;
    while (fread(&fileEntry, sizeof(FileEntry), 1, archiveFile) == 1) {
        // Create the destination file path
        char destFilePath[MAX_FILENAME_LENGTH];
        snprintf(destFilePath, sizeof(destFilePath), "%s/%s", directory, fileEntry.filename);

        // Open the destination file in binary mode
        FILE* destFile = fopen(destFilePath, "wb");
        if (destFile == NULL) {
            printf("Failed to create destination file '%s'.\n", destFilePath);
            perror("Error");
            fclose(archiveFile);
            return 1;
        }

        // Seek to the file data in the archive file
        fseek(archiveFile, fileEntry.offset, SEEK_SET);

        // Copy the file data to the destination file
        char buffer[1024];
        size_t bytesRead;
        long bytesRemaining = fileEntry.size;
        while (bytesRemaining > 0 && (bytesRead = fread(buffer, 1, sizeof(buffer), archiveFile)) > 0) {
            if (bytesRead > bytesRemaining) {
                bytesRead = bytesRemaining;
            }
            if (fwrite(buffer, 1, bytesRead, destFile) != bytesRead) {
                printf("Failed to write file data to destination file '%s'.\n", destFilePath);
                perror("Error");
                fclose(destFile);
                fclose(archiveFile);
                return 1;
            }
            bytesRemaining -= bytesRead;
        }

        // Close the destination file
        fclose(destFile);
        fileCount++;
    }

    // Close the archive file
    fclose(archiveFile);

    printf("Archive '%s' unpacked to directory '%s'.\n", archiveFilename, directory);

    return 0;
}

int add(const char* archiveFilename, const char* filePath) {
    // Open the archive file in read-write mode
    FILE* archiveFile = fopen(archiveFilename, "r+b");
    if (archiveFile == NULL) {
        printf("Failed to open archive file '%s'.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Check if the file already exists in the archive
    FileEntry fileEntry;
    while (fread(&fileEntry, sizeof(FileEntry), 1, archiveFile) == 1) {
        if (strcmp(fileEntry.filename, filePath) == 0) {
            printf("File '%s' already exists in the archive.\n", filePath);
            fclose(archiveFile);
            return 1;
        }
    }

    // Open the source file in binary mode
    FILE* srcFile = fopen(filePath, "rb");
    if (srcFile == NULL) {
        printf("Failed to open source file '%s'.\n", filePath);
        perror("Error");
        fclose(archiveFile);
        return 1;
    }

    // Get the size of the source file
    fseek(srcFile, 0, SEEK_END);
    long size = ftell(srcFile);

    // Create a new file entry
    FileEntry newEntry;
    strncpy(newEntry.filename, filePath, MAX_FILENAME_LENGTH);
    newEntry.offset = ftell(archiveFile);
    newEntry.size = size;

    // Write the new file entry to the archive file
    if (fwrite(&newEntry, sizeof(FileEntry), 1, archiveFile) != 1) {
        printf("Failed to write file entry to archive file '%s'.\n", archiveFilename);
        perror("Error");
        fclose(srcFile);
        fclose(archiveFile);
        return 1;
    }

    // Reset the file position indicator to the beginning of the source file
    rewind(srcFile);

    // Copy the contents of the source file to the archive file
    char buffer[1024];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        if (fwrite(buffer, 1, bytesRead, archiveFile) != bytesRead) {
            printf("Failed to write file data to archive file '%s'.\n", archiveFilename);
            perror("Error");
            fclose(srcFile);
            fclose(archiveFile);
            return 1;
        }
    }

    // Close the files
    fclose(srcFile);
    fclose(archiveFile);

    printf("File '%s' added to archive '%s'.\n", filePath, archiveFilename);

    return 0;
}

int del(const char* archiveFilename, const char* filename) {
    // Open the archive file in read-write mode
    FILE* archiveFile = fopen(archiveFilename, "r+b");
    if (archiveFile == NULL) {
        printf("Failed to open archive file '%s'.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Read the file entries from the archive file
    FileEntry fileEntry;
    int found = 0;
    while (fread(&fileEntry, sizeof(FileEntry), 1, archiveFile) == 1) {
        if (strcmp(fileEntry.filename, filename) == 0) {
            found = 1;
            break;
        }
    }

    if (found) {
        // Remove the file entry from the archive file
        fseek(archiveFile, -sizeof(FileEntry), SEEK_CUR);
        memset(&fileEntry, 0, sizeof(FileEntry));
        if (fwrite(&fileEntry, sizeof(FileEntry), 1, archiveFile) != 1) {
            printf("Failed to delete file entry from archive file '%s'.\n", archiveFilename);
            perror("Error");
            fclose(archiveFile);
            return 1;
        }
    } else {
        printf("File '%s' not found in the archive.\n", filename);
        fclose(archiveFile);
        return 1;
    }

    // Close the archive file
    fclose(archiveFile);

    printf("File '%s' deleted from archive '%s'.\n", filename, archiveFilename);

    return 0;
}

int list(const char* archiveFilename) {
    // Open the archive file in read mode
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Failed to open archive file '%s'.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Read the file entries from the archive file
    FileEntry fileEntry;
    int fileCount = 0;
    while (fread(&fileEntry, sizeof(FileEntry), 1, archiveFile) == 1) {
        printf("File: %s, Size: %ld bytes\n", fileEntry.filename, fileEntry.size);
        fileCount++;
    }

    // Close the archive file
    fclose(archiveFile);

    printf("Total files in archive '%s': %d\n", archiveFilename, fileCount);

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <command> <archive-filename> [arguments...]\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];
    const char* archiveFilename = argv[2];

    if (strcmp(command, "pack") == 0) {
        if (argc < 4) {
            printf("Usage: %s pack <archive-filename> <src-directory>\n", argv[0]);
            return 1;
        }

        const char* srcDirectory = argv[3];
        pack(archiveFilename, srcDirectory);
    } else if (strcmp(command, "unpack") == 0) {
        if (argc < 4) {
            printf("Usage: %s unpack <archive-filename> <dest-directory>\n", argv[0]);
            return 1;
        }

        const char* destDirectory = argv[3];
        unpack(archiveFilename, destDirectory);
    } else if (strcmp(command, "add") == 0) {
        if (argc < 4) {
            printf("Usage: %s add <archive-filename> <file-path>\n", argv[0]);
            return 1;
        }

        const char* filePath = argv[3];
        add(archiveFilename, filePath);
    } else if (strcmp(command, "del") == 0) {
        if (argc < 4) {
            printf("Usage: %s del <archive-filename> <file-name>\n", argv[0]);
            return 1;
        }

        const char* fileName = argv[3];
        del(archiveFilename, fileName);
    } else if (strcmp(command, "list") == 0) {
        list(archiveFilename);
    } else {
        printf("Unknown command '%s'.\n", command);
        return 1;
    }

    return 0;
}
