package main

import (
	"fmt"
	"math/rand"
	"time"
)

const hexCharSet = "abcdefghijklmnopqrstuvwxyz" +
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" +
	"0123456789"

func main() {
	randSource := rand.NewSource(time.Now().UnixNano())
	generate := rand.New(randSource)

	// Want to generate a random 20-character sequence
	strSize := 20
	buf := make([]byte, strSize)
	for i := 0; i < strSize; i++ {
		buf[i] = hexCharSet[generate.Intn(len(hexCharSet))]
	}

	fmt.Printf("Random string is: %s\n", string(buf))

	return
}
