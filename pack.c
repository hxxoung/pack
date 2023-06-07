#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main() {
    const char* new1 = "/home/yun/sysmp_assignment2/part2";

    struct stat st;
    if (stat(new1, &st) == 0) {
        printf("There is a file with the same name.\n");
        return 1;
    }

    int status = mkdir(new1, 0777);

    if (status == 0) {
        printf("Directory '%s' created successfully.\n", new1);
    } else {
        printf("Failed to create directory '%s'.\n", new1);
        perror("Error");
        return 1;
    }

    return 0;
}
