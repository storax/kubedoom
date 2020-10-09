FROM golang:1.14-alpine AS gobuild

WORKDIR /go/src/kubedoom
ADD kubedoom.go .
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o kubedoom .

FROM ubuntu:20.10 AS ubuntu
# make sure the package repository is up to date
RUN apt-get update

FROM ubuntu AS ubuntu-deps
# Install dependencies
RUN apt-get install -y \
  -o APT::Install-Suggests=0 \
  --no-install-recommends \
  wget ca-certificates
RUN wget http://distro.ibiblio.org/pub/linux/distributions/slitaz/sources/packages/d/doom1.wad
RUN wget -O /usr/bin/kubectl https://storage.googleapis.com/kubernetes-release/release/$(wget -O- https://storage.googleapis.com/kubernetes-release/release/stable.txt)/bin/linux/amd64/kubectl \
  && chmod +x /usr/bin/kubectl

FROM ubuntu AS ubuntu-build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get install -y \
  -o APT::Install-Suggests=0 \
  --no-install-recommends \
  build-essential \
  libsdl-mixer1.2-dev \
  libsdl-net1.2-dev \
  gcc

# Setup doom
ADD /dockerdoom /dockerdoom
RUN cd /dockerdoom/trunk && ./configure && make && make install

FROM ubuntu
RUN apt-get install -y \
  -o APT::Install-Suggests=0 \
  --no-install-recommends \
  libsdl-mixer1.2 \
  libsdl-net1.2 \
  x11vnc \
  xvfb \
  netcat-openbsd

WORKDIR /root/

# Setup a password
RUN mkdir ~/.vnc && x11vnc -storepasswd idbehold ~/.vnc/passwd

COPY --from=ubuntu-deps /doom1.wad .
COPY --from=ubuntu-deps /usr/bin/kubectl /usr/bin/
COPY --from=ubuntu-build /usr/local/games/psdoom /usr/local/games/
COPY --from=gobuild /go/src/kubedoom/kubedoom .

ENTRYPOINT ["/root/kubedoom"]
