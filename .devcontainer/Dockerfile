# 使用ubuntu作为基础镜像
FROM ubuntu:latest

# 更新软件源并安装必要的软件包
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libssl-dev \
    bc \
    git \
    curl \
    linux-headers-generic

CMD ["/bin/bash"]
