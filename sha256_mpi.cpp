#include <mpi.h>
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

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    constexpr size_t block_size = 1024 * 1024; // 1MB
    std::vector<uint8_t> block(block_size, 0);
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);

    if (rank == 0) {
        // Master: Read file and distribute blocks
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <file_to_hash>" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        const char* file_name = argv[1];
        std::ifstream file(file_name, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Error opening file: " << file_name << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        size_t num_blocks = (file_size + block_size - 1) / block_size;

        size_t current_block = 0;
        size_t active_workers = std::min(static_cast<size_t>(size - 1), num_blocks);
        std::vector<uint8_t> all_hashes(num_blocks * SHA256_DIGEST_LENGTH);

        // Send initial blocks to workers
        for (int i = 1; i <= active_workers; ++i, ++current_block) {
            file.read(reinterpret_cast<char*>(block.data()), block_size);
            size_t read_bytes = file.gcount();
            MPI_Send(&read_bytes, 1, MPI_UNSIGNED_LONG, i, 0, MPI_COMM_WORLD);
            MPI_Send(block.data(), read_bytes, MPI_BYTE, i, 0, MPI_COMM_WORLD);
        }

        // Receive hashes and send remaining work
        for (size_t i = 0; i < num_blocks; ++i) {
            MPI_Status status;
            MPI_Recv(hash.data(), SHA256_DIGEST_LENGTH, MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            int sender = status.MPI_SOURCE;

            std::memcpy(all_hashes.data() + i * SHA256_DIGEST_LENGTH, hash.data(), SHA256_DIGEST_LENGTH);

            if (current_block < num_blocks) {
                file.read(reinterpret_cast<char*>(block.data()), block_size);
                size_t read_bytes = file.gcount();
                MPI_Send(&read_bytes, 1, MPI_UNSIGNED_LONG, sender, 0, MPI_COMM_WORLD);
                MPI_Send(block.data(), read_bytes, MPI_BYTE, sender, 0, MPI_COMM_WORLD);
                ++current_block;
            } else {
                size_t stop = 0; // Signal no more work
                MPI_Send(&stop, 1, MPI_UNSIGNED_LONG, sender, 1, MPI_COMM_WORLD);
            }
        }

        file.close();

        // Print results
        for (size_t i = 0; i < num_blocks; ++i) {
            std::cout << "Block " << i << " hash: ";
            for (int j = 0; j < SHA256_DIGEST_LENGTH; ++j)
                std::printf("%02x", all_hashes[i * SHA256_DIGEST_LENGTH + j]);
            std::cout << std::endl;
        }

    } else {
        while (true) {
            MPI_Status status;
            size_t chunk_size = 0;

            MPI_Recv(&chunk_size, 1, MPI_UNSIGNED_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == 1 || chunk_size == 0)
                break;  // No more work

            block.resize(chunk_size);
            MPI_Recv(block.data(), chunk_size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            sha256(block.data(), chunk_size, hash.data());
            MPI_Send(hash.data(), SHA256_DIGEST_LENGTH, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
        }
    }

    MPI_Finalize();
    return 0;
}
