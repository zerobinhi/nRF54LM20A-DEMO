cmake_minimum_required(VERSION 3.20.0)

# 寻找 Zephyr 基础包
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# 工程名称
project(nrf54lm20a_board_demo)

# 源码文件
target_sources(app PRIVATE src/main.c)
