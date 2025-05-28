#include <mpi.h>
#include <iostream>
#include <vector>
#include <openssl/sha.h>
#include <fstream>

void sha256(const uint8_t* data, size_t len, uint8_t* out) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data, len);
    SHA256_Final(out, &ctx);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file_to_hash>" << std::endl;
        return 1;
    }

    const char* file_name = argv[1];

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);  // Get the rank of the current process
    MPI_Comm_size(MPI_COMM_WORLD, &size);  // Get the total number of processes

    std::ifstream file(file_name, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error opening file: " << file_name << std::endl;
        MPI_Finalize();
        return 1;
    }

    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    constexpr size_t block_size = 1024 * 1024;  // 1MB
    size_t num_blocks = (file_size + block_size - 1) / block_size;  // Round up to ensure full file is covered

    std::vector<uint8_t> data(block_size, 0);  
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);

    // Scatter the file into blocks (one per rank)
    size_t block_start = rank * block_size;
    size_t block_end = std::min((rank + 1) * block_size, file_size);
    file.seekg(block_start);
    file.read(reinterpret_cast<char*>(data.data()), block_end - block_start);
    file.close();

    sha256(data.data(), block_end - block_start, hash.data());

    std::vector<uint8_t> all_hashes;
    if (rank == 0) {
        all_hashes.resize(size * SHA256_DIGEST_LENGTH);
    }

    // Gather the hashes from all ranks to rank 0
    MPI_Gather(hash.data(), SHA256_DIGEST_LENGTH, MPI_BYTE,
               all_hashes.data(), SHA256_DIGEST_LENGTH, MPI_BYTE,
               0, MPI_COMM_WORLD);

    if (rank == 0) {
        for (int i = 0; i < size; ++i) {
            std::cout << "Rank " << i << " hash: ";
            for (int j = 0; j < SHA256_DIGEST_LENGTH; ++j) {
                std::printf("%02x", all_hashes[i * SHA256_DIGEST_LENGTH + j]);
            }
            std::cout << std::endl;
        }
    }

    MPI_Finalize();
    return 0;
}
