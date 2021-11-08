#!/bin/zsh

pgrep -i Xhttpd | xargs kill -9
rm Xhttpd log/http.log
cp cmake-build-debug/Xhttpd .
./Xhttpd > log/http.log 2>&1 &