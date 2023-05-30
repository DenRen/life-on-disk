#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "../common/file_manip.h"

void prepare_text(std::string text_path) {
    FileDescGuard fd_guard = OpenFile(text_path, O_RDWR);
    int fd = fd_guard.Get();

    uint64_t file_size = GetFileSizeAndSetToBegin(fd);

    char last_char = 0;
    if (read(fd, &last_char, 1) != 1) {
        throw std::runtime_error{"Read failed"};
    }

    if (last_char != '\0') {
        TruncateFile(fd, file_size + 1);

        last_char = '\0';
        if (write(fd, &last_char, 1) != 1) {
            throw std::runtime_error{"Failed to write zero terminated byte"};
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Please, enter file path" << std::endl;
        return 0;
    }

    const char* text_path = argv[1];
    prepare_text(text_path);
    std::cout << "File " << text_path << " has became zero terminated" << std::endl;
}