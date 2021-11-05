#!/bin/zsh

pgrep -i Xhttpd | xargs kill -9
rm Xhttpd http.log
cp cmake-build-debug/Xhttpd .
./Xhttpd > http.log 2>&1 &