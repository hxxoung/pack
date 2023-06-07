#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main() {
    const char* directory = "/path/to/location/new1";

    int status = mkdir(directory, 0777);

    if (status == 0) {
        printf("Directory '%s' created successfully.\n", directory);
    } else {
        printf("Failed to create directory '%s'.\n", directory);
        perror("Error");
        return 1;
    }

    return 0;
}