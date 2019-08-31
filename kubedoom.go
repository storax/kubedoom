package main

//TODO: Make your container die if you die

import (
	"flag"
	//"fmt"
	"log"
	"net"
	"os"
	"os/exec"
	"strings"
	"time"
)

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
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func startCmdSilent(cmdstring string) {
	parts := strings.Split(cmdstring, " ")
	cmd := exec.Command(parts[0], parts[1:len(parts)]...)
	cmd.Stdin = os.Stdin
	err := cmd.Start()
	if err != nil {
		log.Fatalf("The following command failed: \"%v\"\n", cmdstring)
	}
}

func socketLoop(listener net.Listener) {
	log.Print("Socket loop")
	for true {
		log.Print("accept?")
		conn, err := listener.Accept()
		log.Print("accept!")
		if err != nil {
			panic(err)
		}
		stop := false
		for !stop {
			bytes := make([]byte, 40960)
			log.Print("Reading")
			n, err := conn.Read(bytes)
			if err != nil {
				stop = true
			}
			log.Print("got suff")
			bytes = bytes[0:n]
			strbytes := strings.TrimSpace(string(bytes))
			log.Printf("Received: '%s'", strbytes)
			if strbytes == "list" {
				// output := outputCmd(fmt.Sprintf("%v ps -q", dockerBinary))
				args := []string{"kubectl", "get", "pods", "-A", "-o", "go-template", "--template={{range .items}}{{.metadata.namespace}}/{{.metadata.name}} {{end}}"}
				output := outputCmd(args)
				log.Printf("output: '%s'", output)
				_, err = conn.Write([]byte(output))
				if err != nil {
					log.Fatal("Could not write to socker file")
				}
				//cmd := exec.Command("/usr/bin/docker", "inspect", "-f", "{{.Name}}", "`docker", "ps", "-q`")
				// outputstr := strings.TrimSpace(output)
				// pods := strings.Split(outputstr, "\n")
				// for _, pod := range pods {
				// 	log.Print(pod)
				// 	_, err = conn.Write([]byte(pod + " "))
				// 	if err != nil {
				// 		log.Fatal("Could not write to socker file")
				// 	}
				// 	time.Sleep(time.Duration(200) * time.Millisecond)
				// }
				conn.Close()
				stop = true
			} else if strings.HasPrefix(strbytes, "kill ") {
				log.Printf("killcommand: '%s'", strbytes)
				// parts := strings.Split(strbytes, " ")
				// pod := strings.TrimSpace(parts[1])
				// podparts := strings.Split(pod, "/")
				// namespace := podparts[0]

				// podname := podparts[1]
				// log.Printf("Pod to kill: %s // %s", namespace, podname)
				// cmd := exec.Command(dockerBinary, "rm", "-f", docker_id)
				// go cmd.Run()
				conn.Close()
				stop = true
			}
		}
	}
}

func main() {
	var asciiDisplay bool
	flag.BoolVar(&asciiDisplay, "asciiDisplay", false, "Don't use fancy vnc, throw DOOM straightup on my terminal screen")
	flag.Parse()

	listener, err := net.Listen("unix", "/dockerdoom.socket")
	if err != nil {
		log.Fatalf("Could not create socket file")
	}

	if !asciiDisplay {
		log.Print("Create virtual display")
		startCmd("/usr/bin/Xvfb :99 -ac -screen 0 640x480x24")
		time.Sleep(time.Duration(2) * time.Second)
		startCmd("x11vnc -geometry 640x480 -forever -usepw -display :99")
		log.Print("You can now connect to it with a VNC viewer at port 5900")
	}
	log.Print("Trying to start DOOM ...")
	startCmdSilent("/usr/bin/env DISPLAY=:99 /usr/local/games/psdoom -warp -E1M1")
	socketLoop(listener)
}
