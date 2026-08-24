# 通过局域网安装 `.mmpkg`

桌面 Studio 可以将当前选择的 Web 插件打包为 `.mmpkg`，并生成供手机 App 调试入口
扫描的二维码。该通道用于开发阶段的局域网侧载，不提供服务端身份认证。

## 二维码内容

二维码是紧凑 JSON，字段如下：

```json
{"v":1,"scheme":"mmpkg+tcp","host":"192.168.1.8","port":18765,"name":"counter-0.1.0.mmpkg"}
```

- `v`：协议版本，当前为 `1`；
- `scheme`：固定为 `mmpkg+tcp`，不得按固件插件的 `gmp+tcp` 处理；
- `host`、`port`：Studio 的局域网 TCP 地址；
- `name`：建议的下载文件名。

Studio 优先监听 TCP 端口 `18765`；端口被占用时使用随机可用端口，并把实际端口写入
二维码。

## 下载协议

App 连接二维码指定的 TCP 地址后发送：

```text
MMPKG/1 GET\n
```

成功响应为一行 ASCII 元数据，随后紧接包体：

```text
MMPKG/1 OK <size> <sha256>\n<mmpkg bytes>
```

`size` 是十进制字节数，`sha256` 是 64 位小写十六进制摘要。App 必须限制包体为最多
10 MB，读取准确的 `size` 字节，校验 SHA-256，然后再交给正常的 `.mmpkg` manifest、
文件哈希和安装校验流程。

无效请求返回：

```text
MMPKG/1 ERROR invalid-request\n
```

包在下载前被删除、损坏或超过限制时返回：

```text
MMPKG/1 ERROR invalid-package\n
```

## 安全边界

该协议没有 TLS、鉴权或签名，只适用于可信局域网中的 Debug 安装。大小和 SHA-256
能够发现截断或传输不一致，但不能抵抗可同时替换包体和摘要的主动攻击。Release 安装
是否允许该通道，以及 `.mmpkg` 是否必须签名，由 App 的产品安全策略决定。
