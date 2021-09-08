package main

import (
	"fmt"
	"net/http"
)

func HelloServer(w http.ResponseWriter, r *http.Request) {
	fmt.Printf("\nRECEIVED HTTP REQUEST\n")
	fmt.Printf("RequestURI: %s\n", r.URL.RequestURI())
	fmt.Printf("URL.Path: %s\n", r.URL.Path)
	fmt.Printf("URL.RawQuery: %s\n", r.URL.RawQuery)
	fmt.Printf("URL.Query(): %s\n", r.URL.Query())

	fmt.Fprintf(w, "Hello from SERVER-1\n")
}

func main() {
	listenEndpoint := "0.0.0.0:1234"

	fmt.Println("Server listening on:", listenEndpoint)

	http.HandleFunc("/", HelloServer)
	http.ListenAndServe(listenEndpoint, nil)
}
