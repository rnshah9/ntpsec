FROM --platform=linux/amd64 ubuntu:22.04 as builder

RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y python-is-python3 build-essential clang

COPY . /repo
WORKDIR /repo
RUN ./buildprep
RUN ./waf configure
RUN ./waf build
RUN ./waf install 
WORKDIR /repo/build/main/libntp
RUN rm libntp.a
RUN ar rcs libntp.a *.o
RUN clang++ /repo/mayhem/fuzz_eventstr.cpp -fsanitize=fuzzer,address -L /repo/build/main/libntp/ -lntp -o /fuzz

FROM ubuntu:22.04 as package
COPY --from=builder /repo/build/host/ntpd/keyword-gen /
COPY --from=builder /fuzz /fuzz_eventstr
