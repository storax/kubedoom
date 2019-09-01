package main

import (
	"log"
	"net"
	"os"
	"os/exec"
	"strings"
	"time"
	"strconv"
)

func hash(input string) int32 {
	var hash int32
	hash = 5381
	for _, char := range input {
		hash = ((hash << 5) + hash + int32(char))
	}
	if (hash < 0) {
		hash = 0 - hash
	}
	return hash
}

func runCmd(cmdstring string) {
	parts := strings.Split(cmdstring, " ")
	cmd := exec.Command(parts[0], parts[1:len(parts)]...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	err := cmd.Run()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func outputCmd(argv []string) string {
	cmd := exec.Command(argv[0], argv[1:len(argv)]...)
	cmd.Stderr = os.Stderr
	output, err := cmd.Output()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", argv)
	}
	return string(output)
}

func startCmd(cmdstring string) {
	parts := strings.Split(cmdstring, " ")
	cmd := exec.Command(parts[0], parts[1:len(parts)]...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func getPods() []string {
	args := []string{"kubectl", "get", "pods", "-A", "-o", "go-template", "--template={{range .items}}{{.metadata.namespace}}/{{.metadata.name}} {{end}}"}
	output := outputCmd(args)
	outputstr := strings.TrimSpace(output)
	pods := strings.Split(outputstr, " ")
	return pods
}

func socketLoop(listener net.Listener) {
	for true {
		conn, err := listener.Accept()
		if err != nil {
			panic(err)
		}
		stop := false
		for !stop {
			bytes := make([]byte, 40960)
			n, err := conn.Read(bytes)
			if err != nil {
				stop = true
			}
			bytes = bytes[0:n]
			strbytes := strings.TrimSpace(string(bytes))
			pods := getPods()
			if strbytes == "list" {
				for _, pod := range pods {
					padding := strings.Repeat("\n", 255 - len(pod))
					_, err = conn.Write([]byte(pod + padding))
					if err != nil {
						log.Fatal("Could not write to socker file")
					}
				}
				conn.Close()
				stop = true
			} else if strings.HasPrefix(strbytes, "kill ") {
				parts := strings.Split(strbytes, " ")
				killhash, err := strconv.ParseInt(parts[1], 10, 32)
				if err != nil {
					log.Fatal("Could not parse kill hash")
				}
				for _, pod := range pods {
					if (hash(pod) == int32(killhash)) {
						log.Printf("Pod to kill: %v", pod)
						podparts := strings.Split(pod, "/")
						cmd := exec.Command("/usr/bin/kubectl", "delete", "pod", "-n", podparts[0], podparts[1])
						go cmd.Run()
						break
					}
				}
				conn.Close()
				stop = true
			}
		}
	}
}

func main() {
	listener, err := net.Listen("unix", "/dockerdoom.socket")
	if err != nil {
		log.Fatalf("Could not create socket file")
	}

	log.Print("Create virtual display")
	startCmd("/usr/bin/Xvfb :99 -ac -screen 0 640x480x24")
	time.Sleep(time.Duration(2) * time.Second)
	startCmd("x11vnc -geometry 640x480 -forever -usepw -display :99")
	log.Print("You can now connect to it with a VNC viewer at port 5900")

	log.Print("Trying to start DOOM ...")
	startCmd("/usr/bin/env DISPLAY=:99 /usr/local/games/psdoom -warp -E1M1")
	socketLoop(listener)
}
