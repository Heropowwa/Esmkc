#include <asm-generic/ioctl.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <glob.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#define SIZE 960 //idk actually

char* find_device(void) {
    glob_t gstruct;
    int r;
    char *found;
    char device[PATH_MAX];
    char uevent[PATH_MAX];
    char descrp[PATH_MAX];

    FILE *f_uevent;
    char uevent_buf[4096];
    size_t bytes_read;

    FILE *f_descrp;
    char descrp_buf[4096];
    size_t desc_read;
    const unsigned char expected[] = {0x06, 0x89, 0xFF};

    const char *basename;

    size_t ret_len;

    char *result_path;

    r = glob("/sys/class/hidraw/hidraw*", GLOB_ERR , NULL, &gstruct);
    if(r != 0){
        if (r == GLOB_NOMATCH) fprintf(stderr,"No matches\n");
        else fprintf(stderr,"Some kinda glob error\n");
        exit(1);
    }

    //printf("Found %zu filename matches\n", gstruct.gl_pathc);

    for (size_t i = 0; i < gstruct.gl_pathc; i++) {
        found = gstruct.gl_pathv[i];
        //printf("%s\n", found);

        snprintf(device, sizeof(device), "%s/device", found);
        snprintf(uevent, sizeof(uevent), "%s/uevent", device);

        f_uevent = fopen(uevent, "r");
        if (!f_uevent) continue;

        bytes_read = fread(uevent_buf, 1, sizeof(uevent_buf) - 1, f_uevent);
        fclose(f_uevent);
        uevent_buf[bytes_read] = '\0';

        for (size_t j = 0; j < bytes_read; j++) {
            uevent_buf[j] = toupper((unsigned char)uevent_buf[j]);
        }

        //checks for the specific controller
        if (strstr(uevent_buf, "0000048D:0000C615") == NULL) continue;

        snprintf(descrp, sizeof(descrp), "%s/report_descriptor", device);

        f_descrp = fopen(descrp, "rb");
        if (!f_descrp) continue;

        desc_read = fread(descrp_buf, 1, sizeof(descrp_buf), f_descrp);
        fclose(f_descrp);

        //check for three specific bytes
        if (desc_read >= 3 && memcmp(descrp_buf, expected, 3) == 0) {

            basename = strrchr(found, '/');
            basename = basename ? basename + 1 : found;

            ret_len = strlen(basename) + 6; // "/dev/" + null terminator = 6
            result_path = malloc(ret_len);

            if (result_path) {
                snprintf(result_path, ret_len, "/dev/%s", basename);
            }

            break;
        }
    }

    globfree(&gstruct);

    if (result_path != NULL) {
        return result_path;
    }

    fprintf(stderr, "keyboard controller 048d:c615 not found\n");
    exit(1);
}

int send(const char *device_node, uint8_t *buf, size_t len) {
    int fd;
    int ret;

    //Needs sudo or custom udev rule I guess
    fd = open(device_node, O_RDWR);

    if (fd < 0) {
        perror("Error opening device node file, It needs SUDO!!");
        return -1;
    }

    //Plain wizardry here
    ret = ioctl(fd, _IOC(_IOC_READ | _IOC_WRITE, 'H', 6, len), buf);

    if (ret < 0) {
        perror("send ioctl failed");
    }

    close(fd);

    return ret;
}

int query_brightness(const char *device_node) {
    uint8_t buf[SIZE] = {0};
    FILE *f_device_node;
    int fd;
    int ret;

    buf[0] = 0x07;
    buf[1] = 0xCD;
    buf[2] = 0x01;

    send(device_node, buf, SIZE);

    buf[0] = 0x07;

    //Needs sudo or custom udev rule I guess
    f_device_node = fopen(device_node, "rb+");

    if (!f_device_node) {
        perror("Error opening device node file, It needs SUDO!!");
        return -1;
    }

    fd = fileno(f_device_node);

    //Plain wizardry here
    ret = ioctl(fd, _IOC(_IOC_READ | _IOC_WRITE, 'H', 7, 960), buf);

    if (ret < 0) {
        perror("send ioctl failed");
    }

    fclose(f_device_node);

    return buf[4];
}

void set_brightness(const char *device_node, int n) {
    uint8_t buf[SIZE] = {0};

    if ( n < 0 || n > 9) {
        fprintf(stderr, "brightness must be 0-9\n");
        exit(EXIT_FAILURE);
    }

    buf[0] = 0x07;
    buf[1] = 0xCE;
    buf[2] = 0x01;
    buf[3] = 0x00;
    buf[4] = n;

    send(device_node, buf, SIZE);
}

void set_color(const char *device_node, int r, int g, int b) {
    const char *COLOR = "07 cb 18 00 03 01 01 01 06 01 0b 02 00 03 00 04 04 05 02 06 00 01 ff 00 00 01 01 00";
    uint8_t col[SIZE] = {0};
    uint8_t sel[SIZE] = {0};
    size_t count = 0;
    const char *ptr = COLOR;
    char *endptr;
    unsigned long val;
    const int SLOT = 2;

    if (r < 0 || g < 0 || b < 0 || r > 255 || g > 255 || b > 255) {
        fprintf(stderr, "rgb values must be between 0 and 255\n");
        exit(EXIT_FAILURE);
    }

    while (*ptr != '\0') {
        val = strtoul(ptr, &endptr, 16);
        if (ptr == endptr) {
            break;
        }

        if (count < sizeof(col)) {
            col[count++] = (uint8_t)val;
        }

        ptr = endptr;
    }

    col[4] = SLOT;
    col[22] = r;
    col[23] = g;
    col[24] = b;

    sel[0] = 0x07;
    sel[1] = 0xC8;
    sel[2] = 0x01;
    sel[3] = 0x00;
    sel[4] = SLOT;
    send(device_node, sel, SIZE);

    usleep(500000); //enough time from my testing

    send(device_node, col, SIZE);
}

int main(int argc, char *argv[]) {
    /*char *device_node = find_device();
     *    set_brightness(device_node, 9);
     *    int t = query_brightness(device_node);
     *    printf("%d", t);
     *    set_color(device_node, 88, 5, 255);
     *
     *    printf("Successfully found device at: %s\n", device_node);
     *
     *    free(device_node);
     *    return 0;*/

    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s color r g b\n", argv[0]);
        fprintf(stderr, "  %s brightness 0-9\n", argv[0]);
        fprintf(stderr, "  %s status\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *device_node = find_device();
    if (!device_node) {
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "color") == 0) {
        if (argc != 5) {
            fprintf(stderr, "Usage: %s color r g b\n", argv[0]);
            free(device_node);
            return EXIT_FAILURE;
        }
        int r = atoi(argv[2]);
        int g = atoi(argv[3]);
        int b = atoi(argv[4]);

        set_color(device_node, r, g, b);
        printf("Color set to: %d, %d, %d\n", r, g, b);
    }
    else if (strcmp(argv[1], "brightness") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s brightness 0-9\n", argv[0]);
            free(device_node);
            return EXIT_FAILURE;
        }
        int n = atoi(argv[2]);
        set_brightness(device_node, n);
        printf("Brightness set to: %d/9 (%d%%)\n", n, (n * 100 + 4) / 9);
    }
    else if (strcmp(argv[1], "status") == 0) {
        int b = query_brightness(device_node);
        printf("Brightness: %d/9 (%d%%)\n", b, (b * 100 + 4) / 9);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        free(device_node);
        return EXIT_FAILURE;
    }

    free(device_node);
    return EXIT_SUCCESS;
}
