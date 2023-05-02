#include <iostream>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Please, enter file path" << std::endl;
        return 0;
    }

    const char* path = argv[1];

    int fd = open(path, O_RDWR);
    if (fd == -1) {
        std::cerr << "Failed to open file" << std::endl;
        return 0;
    }

    loff_t file_size = lseek64(fd, 0, SEEK_END);
    if (file_size == -1 || lseek64(fd, 0, SEEK_SET) == -1) {
        close(fd);
        std::cerr << "Failed to get file size" << std::endl;
        return -1;
    }

    char last_char = 0;
    if (read(fd, &last_char, 1) != 1) {
        close(fd);
        return -1;
    }

    if (last_char == '\0') {
        close(fd);
        return 0;
    }

    if (ftruncate64(fd, file_size + 1) == -1) {
        close(fd);
        std::cerr << "Failed to truncate" << std::endl;
        return -1;
    }

    last_char = '\0';
    if (write(fd, &last_char, 1) != 1) {
        std::cerr << "Failed to write zero terminated byte" << std::endl;
    }

    close(fd);

    std::cout << "File " << path << " has became zero terminated" << std::endl;
}