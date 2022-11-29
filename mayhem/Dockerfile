FROM --platform=linux/amd64 ubuntu:22.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python-is-python3 build-essential

COPY . /repo
WORKDIR /repo
RUN ./buildprep
RUN ./waf configure
RUN ./waf build
RUN ./waf install 

FROM ubuntu:22.04 as package
COPY --from=builder /repo/build/host/ntpd/keyword-gen /
