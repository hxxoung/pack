#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#define MAX_FILES 100

typedef struct {
    char filename[256];
    int size;
} FileEntry;

int pack(const char* archiveFilename, const char* srcDirectory) {
    printf("Packing files from '%s' into archive '%s'\n", srcDirectory, archiveFilename);

    // Open the source directory
    DIR* dir = opendir(srcDirectory);
    if (dir == NULL) {
        printf("Failed to open source directory '%s'.\n", srcDirectory);
        perror("Error");
        return 1;
    }

    // Create the archive file
    FILE* archiveFile = fopen(archiveFilename, "wb");
    if (archiveFile == NULL) {
        printf("Failed to create archive file '%s'.\n", archiveFilename);
        perror("Error");
        closedir(dir);
        return 1;
    }

    // Read the files in the source directory
    struct dirent* entry;
    FileEntry entries[MAX_FILES];
    int numFiles = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Regular file
            strcpy(entries[numFiles].filename, entry->d_name);

            // Get the file size
            char filePath[256];
            snprintf(filePath, sizeof(filePath), "%s/%s", srcDirectory, entry->d_name);
            struct stat st;
            if (stat(filePath, &st) == 0)
                entries[numFiles].size = st.st_size;
            else
                entries[numFiles].size = -1;

            numFiles++;
        }
    }

    // Close the source directory
    closedir(dir);

    // Write the file entries to the archive file
    fwrite(entries, sizeof(FileEntry), numFiles, archiveFile);

    // Close the archive file
    fclose(archiveFile);

    printf("%d file(s) archived.\n", numFiles);

    return 0;
}

int unpack(const char* archiveFilename, const char* destDirectory) {
    printf("Unpacking files from archive '%s' into directory '%s'\n", archiveFilename, destDirectory);

    // Check if the archive file exists
    FILE* archiveFile = fopen(archiveFilename, "rb");
    if (archiveFile == NULL) {
        printf("Archive file '%s' not found.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Create the destination directory if it doesn't exist
    int status = mkdir(destDirectory, 0777);
    if (status != 0 && errno != EEXIST) {
        printf("Failed to create destination directory '%s'.\n", destDirectory);
        perror("Error");
        fclose(archiveFile);
        return 1;
    }

    // Read the file entries from the archive
    FileEntry entries[MAX_FILES];
    int numFiles = fread(entries, sizeof(FileEntry), MAX_FILES, archiveFile);

    // Close the archive file
    fclose(archiveFile);

    // Unpack the files
    for (int i = 0; i < numFiles; i++) {
        const char* filename = entries[i].filename;
        int fileSize = entries[i].size;

        // Generate the source file path and destination file path
        char srcPath[256];
        snprintf(srcPath, sizeof(srcPath), "%s/%s", destDirectory, filename);
        char destPath[256];
        snprintf(destPath, sizeof(destPath), "%s/%s", destDirectory, filename);

        // Open the source file for reading
        FILE* srcFile = fopen(srcPath, "rb");
        if (srcFile == NULL) {
            printf("Failed to open source file '%s'. Skipping.\n", srcPath);
            continue;
        }

        // Open the destination file for writing
        FILE* destFile = fopen(destPath, "wb");
        if (destFile == NULL) {
            printf("Failed to create destination file '%s'. Skipping.\n", destPath);
            fclose(srcFile);
            continue;
        }

        // Read and write the file contents
        char buffer[1024];
        int bytesRead;
        int totalBytes = 0;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
            fwrite(buffer, 1, bytesRead, destFile);
            totalBytes += bytesRead;
        }

        // Close the files
        fclose(srcFile);
        fclose(destFile);

        // Verify the file size
        if (fileSize != -1 && totalBytes != fileSize) {
            printf("Unpacking file '%s' failed. File size mismatch.\n", filename);
            remove(destPath); // Remove the partially unpacked file
        } else {
            printf("Unpacked file '%s' (%d bytes).\n", filename, totalBytes);
        }
    }

    printf("Unpacking completed.\n");

    return 0;
}

int add(const char* archiveFilename, const char* targetFilename) {
    printf("Adding file '%s' to archive '%s'\n", targetFilename, archiveFilename);

    // Check if the archive file exists
    FILE* archiveFile = fopen(archiveFilename, "rb+");
    if (archiveFile == NULL) {
        printf("Archive file '%s' not found.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Get the file size
    struct stat st;
    if (stat(targetFilename, &st) != 0) {
        printf("Failed to get file size of '%s'.\n", targetFilename);
        perror("Error");
        fclose(archiveFile);
        return 1;
    }
    int fileSize = st.st_size;

    // Check if the file already exists in the archive
    FileEntry entry;
    while (fread(&entry, sizeof(FileEntry), 1, archiveFile) != 0) {
        if (strcmp(entry.filename, targetFilename) == 0) {
            printf("File '%s' already exists in the archive. Skipping.\n", targetFilename);
            fclose(archiveFile);
            return 0;
        }
    }

    // Open the source file for reading
    FILE* srcFile = fopen(targetFilename, "rb");
    if (srcFile == NULL) {
        printf("Failed to open source file '%s'.\n", targetFilename);
        perror("Error");
        fclose(archiveFile);
        return 1;
    }

    // Append the file entry to the archive file
    fseek(archiveFile, 0, SEEK_END);
    strncpy(entry.filename, targetFilename, sizeof(entry.filename));
    entry.size = fileSize;
    fwrite(&entry, sizeof(FileEntry), 1, archiveFile);

    // Copy the file contents to the archive
    char buffer[1024];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        fwrite(buffer, 1, bytesRead, archiveFile);
    }

    // Close the files
    fclose(srcFile);
    fclose(archiveFile);

    printf("File '%s' added to the archive.\n", targetFilename);

    return 0;
}

int del(const char* archiveFilename, const char* targetFilename) {
    printf("Deleting file '%s' from archive '%s'\n", targetFilename, archiveFilename);

    // Check if the archive file exists
    FILE* archiveFile = fopen(archiveFilename, "rb+");
    if (archiveFile == NULL) {
        printf("Archive file '%s' not found.\n", archiveFilename);
        perror("Error");
        return 1;
    }

    // Search for the file entry in the archive
    FileEntry entry;
    int found = 0;
    while (fread(&entry, sizeof(FileEntry), 1, archiveFile) != 0) {
        if (strcmp(entry.filename, targetFilename) == 0) {
            found = 1;
            break;
        }
    }

    if (found) {
        // Calculate the size of the file contents
        int fileSize = entry.size;

        // Calculate the offset of the file entry
        long offset = -sizeof(FileEntry);

        // Delete the file entry from the archive
        fseek(archiveFile, offset, SEEK_CUR);
        fwrite(&entry, sizeof(FileEntry), 1, archiveFile);

        // Remove the file contents from the archive
        fseek(archiveFile, fileSize, SEEK_CUR);
        fwrite("", 1, fileSize, archiveFile);

        printf("File '%s' deleted from the archive.\n", targetFilename);
    } else {
        printf("File '%s' not found in the archive.\n", targetFilename);
    }

    // Close the archive file
    fclose(archiveFile);

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("To pack: %s pack archive-filename src-directory\n", argv[0]);
        printf("To unpack: %s unpack archive-filename dest-directory\n", argv[0]);
        printf("To add: %s add archive-filename target-filename\n", argv[0]);
        printf("To delete: %s del archive-filename target-filename\n", argv[0]);
        return 1;
    }

    const char* command = argv[1];
    const char* archiveFilename = argv[2];

    if (strcmp(command, "pack") == 0) {
        const char* srcDirectory = argv[3];
        return pack(archiveFilename, srcDirectory);
    } else if (strcmp(command, "unpack") == 0) {
        const char* destDirectory = argv[3];
        return unpack(archiveFilename, destDirectory);
    } else if (strcmp(command, "add") == 0) {
        const char* targetFilename = argv[3];
        return add(archiveFilename, targetFilename);
    } else if (strcmp(command, "del") == 0) {
        const char* targetFilename = argv[3];
        return del(archiveFilename, targetFilename);
    } else {
        printf("Invalid command.\n");
        return 1;
    }
}
