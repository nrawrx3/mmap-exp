package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"time"

	mmapexp "github.com/nrawrx3/mmap-exp"
)

type Flags struct {
	BackingFile string
}

var flags Flags

func main() {
	flag.StringVar(&flags.BackingFile, "file", "", "backing file")
	flag.Parse()

	if flags.BackingFile == "" {
		fmt.Fprintf(os.Stderr, "expected a file name for argument -file")
		os.Exit(1)
	}

	man, err := mmapexp.NewManager(mmapexp.CreateOptions{
		BackingFile:             flags.BackingFile,
		InitialReservedSize:     0,
		ReserveExistingFileSize: true,
	})
	if err != nil {
		log.Fatal(err)
	}

	const seconds = 20

	// See the mapped region in /proc
	log.Printf("Waiting for 20 secs: PID: %d", os.Getpid())

	man.MapNextFileChunk(mmapexp.MapNextFileChunkOptions{
		DontGrowIfFullyMapped: true,
		ChunksToMapNext:       64,
	})

	<-time.After(seconds * time.Second)

	err = man.Delete()
	if err != nil {
		log.Fatal(err)
	}
}
