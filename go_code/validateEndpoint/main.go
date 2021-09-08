package main

import (
	"fmt"
	"os"
	"regexp"
	"strconv"
	"strings"
)

// Validates an IPv4 TCP or UDP endpoint address
func validateEndpoint(ep string) bool {
	epSplit := strings.Split(ep, ":")
	if len(epSplit) != 2 {
		return false
	}

	ip, portStr := epSplit[0], epSplit[1]

	// Validate IP
	// Allow endpoints with just the port number (e.g. ":1234")
	if ip != "" {
		match, err := regexp.Match("[0-9.]", []byte(ip))
		if match != true || err != nil {
			return false
		}

		ipSplit := strings.Split(ip, ".")
		if len(ipSplit) != 4 {
			return false
		}

		for _, octetStr := range ipSplit {
			octet, err := strconv.Atoi(octetStr)
			if octet < 0 || octet > 255 || err != nil {
				return false
			}
		}
	}

	// Validate port number
	port, err := strconv.Atoi(portStr)
	if err != nil {
		return false
	}

	if port < 1 || port > 65535 {
		return false
	}

	return true
}

func main() {
	if len(os.Args) != 2 {
		fmt.Println("Expecting at least one argument (IPv4 Endpoint)")
		os.Exit(1)
	}

	properAddr := validateEndpoint(os.Args[1])
	if properAddr == false {
		fmt.Println("Bad endpoint")
	} else {
		fmt.Println("Valid endpoint")
	}

	return
}
