mmap-example.bin: ./cmd/example/main.go
	go build -o mmap-example.bin ./cmd/example/...

run:
	LD_LIBRARY_PATH=./mmapext/build:$$LD_LIBRARY_PATH ./mmap-example.bin -file backing
