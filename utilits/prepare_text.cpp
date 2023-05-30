#include <iostream>
#include <unistd.h>
#include <fcntl.h>

#include "../common/file_manip.h"

int main(int argc, char* argv[]) try {
    if (argc == 2) {
        const char* text_path = argv[1];
        MakeZeroTerminated(text_path);
        std::cout << "File " << text_path << " has became zero terminated" << std::endl;
    } else {
        std::cerr << "Please, enter file path" << std::endl;
    }
} catch (std::exception& exc) {
    std::cerr << "Exception: " << exc.what() << std::endl;
}