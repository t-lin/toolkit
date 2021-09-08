package main

import (
	"fmt"
	"os"
	"os/signal"
	"time"
)

func sigHandler(sigCh chan os.Signal) {
	for sig := range sigCh {
		switch sig {
		case os.Interrupt:
			fmt.Printf("Interrupt signal caught! Bye!\n")
			time.Sleep(time.Second) // Allow flush to file if re-directing
			os.Exit(0)
		default:
			fmt.Printf("Unknown signal caught! %d\n", sig)
		}
	}
}

func main() {
	sigCh := make(chan os.Signal, 1)

	// NOTE: Interrupt (from Ctrl+C) is the only catchable signal aliased in 'os'
	//		 Other signals are in 'syscall' package
	signal.Notify(sigCh, os.Interrupt) // Could use syscall.SIGINT

	// To catch all signals, omit any signal arguments, e.g.
	//	- signal.Notify(sigCh)

	// Can put sigHandler into a background goroutine
	sigHandler(sigCh)
}
