#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main() {
    const char* directory = "/home/yun/sysmp_assignment2/part2/new1";

    int status = mkdir(directory, 0777);

    if (status == 0) {
        printf("Directory 'new1' created successfully at the specified location.\n");
    } else {
        printf("Failed to create directory 'new1' at the specified location.\n");
        perror("Error");
        return 1;
    }

    return 0;
}
