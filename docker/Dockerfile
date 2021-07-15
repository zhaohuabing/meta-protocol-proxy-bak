FROM ubuntu:18.04

COPY envoy-trpc-proxy /usr/bin/
ADD trpc-static.yaml /etc/

CMD /usr/bin/envoy-trpc-proxy -c /etc/trpc-static.yaml