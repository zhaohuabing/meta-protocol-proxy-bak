#!/bin/bash
cd helloworld/server
go build 
./trpc.app.Greeter&
cd ../client
go build
./trpc.app.Greeter -addr=ip://127.0.0.1:28000
echo "test http"
curl -v  -H 'host:www.test.com' http://127.0.0.1:28001
