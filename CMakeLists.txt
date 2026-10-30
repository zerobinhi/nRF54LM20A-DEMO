cmake_minimum_required(VERSION 3.20.0)

# 寻找 Zephyr 基础包
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 工程名称
project(nRF54LM20A-DEMO)

# 源码文件
target_sources(app PRIVATE src/main.c)
