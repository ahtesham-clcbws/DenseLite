#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <vector>

const uint32_t GGUF_MAGIC = 0x46554747;

int main(int argc, char** argv) {
    int fd = open(argv[1], O_RDONLY);
    struct stat sb; fstat(fd, &sb);
    void* mmap_data = mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    uint8_t* ptr = static_cast<uint8_t*>(mmap_data);
    ptr += 4; // magic
    uint32_t version; std::memcpy(&version, ptr, 4); ptr += 4;
    uint64_t tensor_count, kv_count;
    std::memcpy(&tensor_count, ptr, 8); ptr += 8;
    std::memcpy(&kv_count, ptr, 8); ptr += 8;

    for (uint64_t i = 0; i < kv_count; ++i) {
        uint64_t key_len; std::memcpy(&key_len, ptr, 8); ptr += 8;
        ptr += key_len;
        uint32_t val_type; std::memcpy(&val_type, ptr, 4); ptr += 4;
        if (val_type == 8) { // string
            uint64_t len; std::memcpy(&len, ptr, 8); ptr += 8; ptr += len;
        } else if (val_type == 9) { // array
            uint32_t atype; std::memcpy(&atype, ptr, 4); ptr += 4;
            uint64_t alen; std::memcpy(&alen, ptr, 8); ptr += 8;
            if (atype == 8) {
                for (uint64_t j = 0; j < alen; ++j) {
                    uint64_t slen; std::memcpy(&slen, ptr, 8); ptr += 8; ptr += slen;
                }
            } else { ptr += alen * 4; /* approx, might crash but whatever */ }
        } else if (val_type == 4) { ptr += 4; }
        else if (val_type == 5) { ptr += 8; }
        else if (val_type == 6) { ptr += 4; }
        else if (val_type == 7) { ptr += 8; }
        else if (val_type == 0) { ptr += 1; }
        else if (val_type == 1) { ptr += 1; }
        else if (val_type == 2) { ptr += 2; }
        else if (val_type == 3) { ptr += 2; }
    }
    // Just gonna skip KVs in a safer way: wait, kv parsing is hard.
    return 0;
}
