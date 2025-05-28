#include <iostream>
#include <vector>
#include <openssl/sha.h>
#include <fstream>
#include <cstring>

void sha256(const uint8_t* data, size_t len, uint8_t* out) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(out, &ctx);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file_to_hash>" << std::endl;
        return 1;
    }

    const char* file_name = argv[1];

    std::ifstream file(file_name, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error opening file: " << file_name << std::endl;
        return 1;
    }

    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    constexpr size_t block_size = 1024 * 1024; // 1MB
    size_t num_blocks = (file_size + block_size - 1) / block_size;

    std::vector<std::vector<uint8_t>> hashes(num_blocks, std::vector<uint8_t>(SHA256_DIGEST_LENGTH));

    for (size_t i = 0; i < num_blocks; ++i) {
        std::vector<uint8_t> block(block_size, 0);
        file.read(reinterpret_cast<char*>(block.data()), block_size);
        size_t read_bytes = file.gcount();
        sha256(block.data(), read_bytes, hashes[i].data());
    }

    file.close();

    for (size_t i = 0; i < num_blocks; ++i) {
        std::cout << "Hash " << i << ": ";
        for (auto b : hashes[i]) {
            std::printf("%02x", b);
        }
        std::cout << std::endl;
    }

    return 0;
}
