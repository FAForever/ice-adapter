FROM ubuntu:16.10
MAINTAINER Martin Muellenhaupt "mm+fafsdpserver@netlair.de"

RUN apt-get update && apt-get install -y qtbase5-dev libboost-dev ninja-build libjsoncpp-dev cmake g++ git libboost-log-dev libboost-program-options-dev libboost-thread-dev libboost-system-dev libboost-date-time-dev

RUN git clone https://github.com/FAForever/ice-adapter.git && \
    mkdir build && \
    cd build && \
    cmake -G "Ninja" -DFAF_BUILD_TEST=ON ../ice-adapter && \
    ninja faf-ice-testserver

EXPOSE 54321
ENTRYPOINT ["build/faf-ice-testserver"]
