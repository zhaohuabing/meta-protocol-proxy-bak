# envoy-trpc 

支持 trpc 的微服务网关和服务网格支持。

## 项目介绍
1. 扩展 envoyproxy 支持 trpc 的协议支持。
2. 实现支持 trpc 协议的 apigateway 和 sidecar。

## 快速开始

### 指引
[envoyproxy自定义协议扩展](http://km.oa.com/group/34294/articles/show/419807)

### 编译

```bash
    ./build.sh
```

### 运行

```bash
    ./run.sh
```

### 测试

```bash
    cd test
    ./test.sh
```

### 流水线

[蓝盾编译流水线](http://devops.oa.com/console/pipeline/istio/p-90a14ae7e817439fb725cfdabb34550f/)

### 性能测试

性能测试目前主要是以go程序来做proxy压测，需要提前准备好golang环境

#### 基础数据

在 AMD EPYC 7K62 48-Core Processor 的devnet机器上，实现单核2.7万/s的转发性能，欢迎贡献更多的测试数据。

#### 基础对比测试
首先执行 ./run.sh 脚本启动 envoyproxy

```bash
cd test/helloworld/server
go test -bench=.
```

#### 压测

通过流水线下载最新的 envoyproxy-trpc-proxy，如果需要自行编译需要使用bazel的opt参数生成release包，否则数据不可信
可以使用现有镜像:
```
ccr.ccs.tencentyun.com/istio-testing/envoy-trpc-proxy
```

启动服务端
```bash
cd test/helloworld/server
go build
./trpc.app.Greeter
```
启动客户端
```bash
cd test/helloworld/client
go build
./trpc.app.Greeter -addr=ip://127.0.0.1:28000 -n 1000000 -i 0 -c 100
```
通过调整参数实现不同的压力
参数说明
```
Usage of ./trpc.app.Greeter:
  -addr string
        addr, supporting ip://<ip>:<port>, l5://mid:cid, cmlb://appid[:sysid] (default "ip://127.0.0.1:8000")
  -c int
        -c 10, concurrency number (default 1)
  -cmd string
        cmd SayHello (default "SayHello")
  -i int
        -i 10, send interval (default 1000)
  -n int
        -n 10 number for requests (default 1)
```

### 功能介绍

#### RDS

这个RDS方案是快速实现版本；把tRPC和http rds的路由做映射，这样不用怎么改pilot只需要识别tRPC协议就可以使用了。

映射关系：
- headers:
    - path: 即tRPC协议中的RequestProtocol.func。
    - host: 由tRPC协议中的RequestProtocol.callee删除接口名得到的。
    - 其他: 由tRPC协议中的RequestProtocol.trans_info复制来的。

#### 主动健康检查
目前只支持建立tcp连接形式的健康检查；等到tRPC框架确定了心跳包方案后，这里会跟进补充基于心跳包形式的健康检查。

yaml配置如下
```yaml
  clusters:
    health_checks:
      - custom_health_check:
          name: envoy.health_checkers.trpc
          typed_config:
            "@type": type.googleapis.com/envoy.config.health_checker.trpc_proxy.v3.Trpc
            only_verify_connect: true
            callee: xxxxxxxxx
            caller: xxxxxxxxx
```

主要要填三个字段(定义在trpc/health_checker/trpc.proto文件)

- only_verify_connect： 如果only_verify_connect是true，仅仅验证socket是否可以成功连接；否则会发心跳包(暂不支持)。
- callee： 发送tRPC心跳包时使用的主调名(暂不支持)。
- caller： 发送tRPC心跳包时使用的被调名(暂不支持)。

#### 连接策略

目前支持三种策略：轮询、基于downstream ip hash、基于http headers hash

##### 轮询

yaml配置如下

```yaml
  clusters:
    name: xxx
    type: STATIC
    lb_policy: ROUND_ROBIN
```

##### 基于downstream ip hash

filters配置上hash_policy，并指明使用source_ip

```yaml
filter_chains:
    - filters:
      - name: envoy.trpc_proxy
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.trpc_proxy.v3.TrpcProxy
          route_config:
            name: xxxxxxxxx
            virtual_hosts:
            - name: local_service
              domains: ["trpc.test.helloworld"]
              routes:
              - match:
                  prefix: "/trpc.test.helloworld.Greeter/"
                route:
                  cluster: trpc.test.helloworld
                  hash_policy:
                  - connection_properties:
                      source_ip: true
```

clusters的lb_policy要配置成RING_HASH

```yaml
  clusters:
    name: xxx
    type: STATIC
    lb_policy: RING_HASH
```

##### 基于http headers hash

http headers是由tRPC协议中的RequestProtocol.trans_info复制而来的；如果需要自定义headers字段，在downstream把信息填入tRPC协议中的RequestProtocol.trans_info即可。

filters配置上hash_policy，并指明要使用的header_name

```yaml
filter_chains:
    - filters:
      - name: envoy.trpc_proxy
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.trpc_proxy.v3.TrpcProxy
          route_config:
            name: xxxxxxxxx
            virtual_hosts:
            - name: xxxxxx
              domains: ["trpc.test.helloworld"]
              routes:
              - match:
                  prefix: "/trpc.test.helloworld.Greeter/"
                route:
                  cluster: trpc.test.helloworld
                  hash_policy:
                  - header:
                      header_name: "xxxxxxxxx"
```

clusters的lb_policy要配置成RING_HASH

```yaml
  clusters:
    name: xxx
    type: STATIC
    lb_policy: RING_HASH
```

#### AccessLog

##### 使用说明

适配envoy.extensions.access_loggers.file.v3.[FileAccessLog](https://www.envoyproxy.io/docs/envoy/latest/api-v3/config/accesslog/v3/accesslog.proto#envoy-v3-api-msg-config-accesslog-v3-accesslog)，仅支持部分ormat Strings支持的字段。


##### [Format Strings](https://www.envoyproxy.io/docs/envoy/latest/configuration/observability/access_log/usage#config-access-log-default-format)支持的字段
 - %START_TIME% : 请求开始时间
 - %REQ(X-ENVOY-ORIGINAL-PATH?:PATH)% : tRPC协议中的RequestProtocol.fun
 - %RESPONSE_CODE% : 返回值（tRPC的返回码）
 - %RESPONSE_FLAGS% : 返回和连接的附加信息
 - %BYTES_RECEIVED% : 收到的字节数（包括帧头和包头）
 - %BYTES_SENT% : 回复的字节数（包括帧头和包头）
 - %DURATION% : 从开始到最后一个字节发出的请求的总持续时间（以毫秒为单位
 - %UPSTREAM_HOST% : 上游主机URL
 - %UPSTREAM_CLUSTER% : 上游主机所属的cluster
 - %UPSTREAM_LOCAL_ADDRESS% : 上游连接的本地地址。如果该地址是IP地址，则包括地址和端口。
 - %DOWNSTREAM_REMOTE_ADDRESS% 下游连接的远程地址。如果该地址是IP地址，则包括地址和端口。
 - %DOWNSTREAM_LOCAL_ADDRESS : 下游连接的本地地址。如果该地址是IP地址，则包括地址和端口。
 
 for example:
 ```yaml
    filter_chains:
    - filters:
      - name: envoy.trpc_proxy
        typed_config:
          "@type": type.googleapis.com/envoy.config.filter.network.trpc_proxy.v3.TrpcProxy
          access_log:
            - name: envoy.access_loggers.file
              typed_config:
                "@type": type.googleapis.com/envoy.extensions.access_loggers.file.v3.FileAccessLog
                path: "/dev/stdout"
                log_format:
                  text_format: "[%START_TIME%] %REQ(X-ENVOY-ORIGINAL-PATH?:PATH)% %RESPONSE_CODE% %RESPONSE_FLAGS% %BYTES_RECEIVED% %BYTES_SENT% %DURATION% %UPSTREAM_HOST%\n"

```