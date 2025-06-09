CC=clang++
MPICXX=mpicxx
CFLAGS=-I/opt/homebrew/opt/openssl@3/include -Wno-deprecated-declarations
LFLAGS=-L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

OPENMP_FLAGS=-Xpreprocessor -fopenmp \
          -I/opt/homebrew/Cellar/libomp/20.1.6/include \
          -L/opt/homebrew/Cellar/libomp/20.1.6/lib -lomp

FILE=testdata.bin

all: openmp mpi sha256

openmp: sha256_openmp.cpp
	$(CC) -std=c++11 sha256_openmp.cpp $(CFLAGS) $(OPENMP_FLAGS) $(LFLAGS) -o sha256_openmp

mpi: sha256_mpi.cpp
	$(MPICXX) -std=c++11 sha256_mpi.cpp $(CFLAGS) $(LFLAGS) -o sha256_mpi

sha256: sha256.cpp
	$(CC) -std=c++11 sha256.cpp $(CFLAGS) $(LFLAGS) -o sha256

gen:
	dd if=/dev/urandom of=$(FILE) bs=1G count=1 status=none

clean:
	rm -f sha256_openmp sha256_mpi sha256 $(FILE)
	
run: gen all
	@echo "\n🔁 Running OpenMP SHA256:"
	@./sha256_openmp $(FILE)

	@echo "\n🔁 Running MPI SHA256:"
	@mpirun -np 4 ./sha256_mpi $(FILE)

	@echo "\n🔁 Running SHA256:"
	@./sha256 $(FILE)

benchmark: gen all
	# @echo "\n⚡ Benchmarking OpenMP SHA256:"
	# @time ./sha256_openmp $(FILE)

	@echo "\n⚡ Benchmarking MPI SHA256:"
	@time mpirun -np 4 ./sha256_mpi $(FILE)

	# @echo "\n⚡ Benchmarking SHA256:"
	# @time ./sha256 $(FILE)
