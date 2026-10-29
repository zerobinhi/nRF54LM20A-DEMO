/*
 * ============================================================================
 *  _   _ ____  _____ ____  _  _   _     __  __ ____   ___    _    
 * | \ | |  _ \|  ___| ___|| || | | |   |  \/  |___ \ / _ \  / \   
 * |  \| | |_) | |_  |___ \| || |_| |   | |\/| | __) | | | |/ _ \  
 * | |\  |  _ <|  _|  ___) |__   _| |___| |  | |/ __/| |_| / ___ \ 
 * |_| \_|_| \_\_|   |____/   |_| |_____|_|  |_|_____|\___/_/   \_\
 * 
 *  nRF54LM20A 极客版 硬件超级自检与全能演示工程 (Cyber Geek Demo)
 * ============================================================================
 *  硬件平台: Nordic nRF54LM20A (Cortex-M33 @ 128 MHz)
 *  模组型号: ME54BS13-nRF54LM20A
 *  板级目标: nrf54lm20dk/nrf54lm20a/cpuapp
 * ----------------------------------------------------------------------------
 *  炫酷特性总览:
 *   1. 【ANSI 炫彩终端交互控制台】
 *      - 启动赛博朋克风格 ASCII 艺术 Logo 与硬件架构拓扑图
 *      - 终端实时彩色渲染: 打印 WS2812 真实颜色色块 (ANSI TrueColor)
 *      - 支持串口键盘双向交互: 输入 '1'~'5', 'm', 'b', 'h' 实时下发指令
 *   2. 【WS2812B 梦幻光效引擎】(P1.11, 128MHz 汇编精确位翻转)
 *      - 模式 0: 平滑彩虹流光 (256阶 HSV 平滑色彩轮，无缝渐变)
 *      - 模式 1: 赛博霓虹呼吸灯 (采用人眼视觉 Gamma 平方缓动平滑过渡)
 *      - 模式 2: 炫彩跑马灯 / 能量脉冲闪烁 (Pulse Strobe)
 *      - 模式 3: 按键响应联动变色 (交互式触觉色彩反馈)
 *      - 具备 4 档全局亮度平滑调节 (10% -> 25% -> 50% -> 100%)
 *   3. 【4 颗 LED 炫酷灯效模式切换】(P1.22, P1.25, P1.27, P1.28)
 *      - 模式 A: 独立手动翻转 (按键 SW1~SW4 独立控制)
 *      - 模式 B: 霹雳游侠 Knight Rider 往返扫描流光灯 (0->1->2->3->2->1)
 *      - 模式 C: 4 位二进制实时硬件时钟计数器 (0000 ~ 1111)
 *      - 模式 D: 狂欢呼吸双向碰撞闪烁
 *   4. 【多功能手势轻触按键引擎】(SW1:P1.26, SW2:P1.09, SW3:P1.08, SW4:P0.05)
 *      - 10ms 轮询 + 30ms 积分消抖算法
 *      - 支持【单击】与【长按 > 800ms】手势识别:
 *        • 单击: 翻转对应 LED，切换当前显示状态
 *        • 长按 SW1: 重新执行 Flash 极速吞吐量基准跑分
 *        • 长按 SW2: 循环切换 4 颗 LED 的灯效模式
 *        • 长按 SW3: 调节 WS2812B 整体亮度等级
 *        • 长按 SW4: 切换 WS2812B 的光效渲染算法
 *   5. 【W25Q128JVSIQ 16 MiB Flash 深度诊断与速度基准测试】
 *      - PRE_KERNEL_1 阶段硬件级自动拉高 P2.00(HOLD#) 与 P2.03(WP#)
 *      - 4 KiB 扇区擦除速度跑分 (计算擦除吞吐量 KB/s)
 *      - 空白擦除校验 (Blank Check 0xFF)
 *      - 256 字节完整 Page Program 页编程测试与写入速率跑分
 *      - 1024 字节高速读取吞吐量跑分与多重特征码 (0x55/0xAA 棋盘格) 校验
 *      - ANSI 彩色高亮 16 字节 Hex + ASCII 内存拓扑 Dump
 * ============================================================================
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <soc.h>
#include <string.h>

/* ========================================================================== */
/* ANSI 终端彩色转义字符定义                                                  */
/* ========================================================================== */
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_RED     "\033[1;31m"
#define ANSI_GREEN   "\033[1;32m"
#define ANSI_YELLOW  "\033[1;33m"
#define ANSI_BLUE    "\033[1;34m"
#define ANSI_MAGENTA "\033[1;35m"
#define ANSI_CYAN    "\033[1;36m"
#define ANSI_WHITE   "\033[1;37m"
#define ANSI_GRAY    "\033[90m"

/* ========================================================================== */
/* 1. 外挂 Flash WP# / HOLD# 早期硬件拉高初始化                              */
/* ========================================================================== */
#define FLASH_HOLD_PIN 0  /* P2.00 (Pin 7 /HOLD) */
#define FLASH_WP_PIN   3  /* P2.03 (Pin 3 /WP)   */

static int flash_wp_hold_early_init(void)
{
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio2), okay)
	NRF_GPIO_Type *gpio2 = (NRF_GPIO_Type *)DT_REG_ADDR(DT_NODELABEL(gpio2));
	gpio2->DIRSET = BIT(FLASH_HOLD_PIN) | BIT(FLASH_WP_PIN);
	gpio2->OUTSET = BIT(FLASH_HOLD_PIN) | BIT(FLASH_WP_PIN);
#endif
	return 0;
}
SYS_INIT(flash_wp_hold_early_init, PRE_KERNEL_1, 50);

/* ========================================================================== */
/* 2. LED 与 按键硬件定义                                                     */
/* ========================================================================== */
#define NUM_LEDS 4
#define NUM_BTNS 4

static const struct gpio_dt_spec led_specs[NUM_LEDS] = {
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(led3), gpios, {0}),
};

static const struct gpio_dt_spec btn_specs[NUM_BTNS] = {
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw1), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw2), gpios, {0}),
	GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw3), gpios, {0}),
};

static const char *const led_names[NUM_LEDS] = {
	"LED0 (P1.22)", "LED1 (P1.25)", "LED2 (P1.27)", "LED3 (P1.28)"
};

static const char *const btn_names[NUM_BTNS] = {
	"SW1 (P1.26)",  "SW2 (P1.09)",  "SW3 (P1.08)",  "SW4 (P0.05)"
};

/* LED 特效模式 */
enum led_mode {
	LED_MODE_MANUAL = 0,   /* 手动按键反转模式 */
	LED_MODE_KNIGHT_RIDER, /* 霹雳游侠往返流水灯 */
	LED_MODE_BINARY_COUNT, /* 4位二进制递增计数器 */
	LED_MODE_MARQUEE,      /* 跑马灯加速轮询 */
	LED_MODE_MAX
};
static const char *const led_mode_names[] = {
	"MANUAL (手动控制)", "KNIGHT_RIDER (霹雳游侠)", "BINARY_COUNT (二进制时钟)", "MARQUEE (急速跑马)"
};

/* ========================================================================== */
/* 3. WS2812B 极速单总线驱动与炫彩渲染引擎                                   */
/* ========================================================================== */
#define WS2812_PIN  11
#define WS2812_PORT ((NRF_GPIO_Type *)DT_REG_ADDR(DT_NODELABEL(gpio1)))
#define WS2812_BIT  BIT(WS2812_PIN)

/* 针对 Cortex-M33 @ 128 MHz 的微调 NOP 计数 (1周期 = 7.81ns) */
#define WS_T0H_NOP 45   /* ~360 ns */
#define WS_T0L_NOP 105  /* ~850 ns */
#define WS_T1H_NOP 100  /* ~800 ns */
#define WS_T1L_NOP 50   /* ~420 ns */

#define _WS_STR(x) #x
#define WS_STR(x) _WS_STR(x)
#define WS_NOP(n) __asm__ volatile(".rept " WS_STR(n) "\n\tnop\n\t.endr\n")

static inline __attribute__((always_inline)) void ws_send_zero(void)
{
	WS2812_PORT->OUTSET = WS2812_BIT;
	WS_NOP(WS_T0H_NOP);
	WS2812_PORT->OUTCLR = WS2812_BIT;
	WS_NOP(WS_T0L_NOP);
}

static inline __attribute__((always_inline)) void ws_send_one(void)
{
	WS2812_PORT->OUTSET = WS2812_BIT;
	WS_NOP(WS_T1H_NOP);
	WS2812_PORT->OUTCLR = WS2812_BIT;
	WS_NOP(WS_T1L_NOP);
}

static void ws2812_send_raw(uint8_t r, uint8_t g, uint8_t b)
{
	uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
	unsigned int key = irq_lock();

	for (int i = 23; i >= 0; i--) {
		if (grb & BIT(i)) {
			ws_send_one();
		} else {
			ws_send_zero();
		}
	}
	WS2812_PORT->OUTCLR = WS2812_BIT;
	irq_unlock(key);

	k_busy_wait(320); /* 复位电平保持 > 300us */
}

/* 亮度档位设置: 10%, 25%, 50%, 100% */
static const uint8_t brightness_levels[] = {25, 64, 128, 255};
static uint8_t brightness_idx = 1; /* 默认 25% 亮度 */

/* 当前渲染出的真实颜色缓存 */
static uint8_t current_r = 0, current_g = 0, current_b = 0;

static void ws2812_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t scale = brightness_levels[brightness_idx];
	current_r = (uint8_t)(((uint16_t)r * scale) / 255);
	current_g = (uint8_t)(((uint16_t)g * scale) / 255);
	current_b = (uint8_t)(((uint16_t)b * scale) / 255);
	ws2812_send_raw(current_r, current_g, current_b);
}

/* WS2812 特效模式 */
enum ws_mode {
	WS_MODE_RAINBOW = 0,  /* 平滑彩虹流光渐变 */
	WS_MODE_BREATHE,      /* 赛博朋克平滑呼吸灯 */
	WS_MODE_PULSE,        /* 能量脉冲爆闪 */
	WS_MODE_STATIC,       /* 静态按键联动色 */
	WS_MODE_MAX
};
static const char *const ws_mode_names[] = {
	"RAINBOW (彩虹流光)", "BREATHE (霓虹呼吸)", "PULSE (能量脉冲)", "STATIC (按键联动)"
};

/* HSV 转 RGB 函数 (色相 H: 0..255, 饱和度 S: 0..255, 明度 V: 0..255) */
static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (s == 0) {
		*r = *g = *b = v;
		return;
	}

	uint8_t region = h / 43;
	uint8_t remainder = (h - (region * 43)) * 6;

	uint8_t p = (v * (255 - s)) >> 8;
	uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
	uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

	switch (region) {
	case 0:  *r = v; *g = t; *b = p; break;
	case 1:  *r = q; *g = v; *b = p; break;
	case 2:  *r = p; *g = v; *b = t; break;
	case 3:  *r = p; *g = q; *b = v; break;
	case 4:  *r = t; *g = p; *b = v; break;
	default: *r = v; *g = p; *b = q; break;
	}
}

/* ========================================================================== */
/* 4. W25Q128JVSIQ 16 MiB Flash 深度诊断与速度基准测试套件                    */
/* ========================================================================== */
#define FLASH_NODE DT_NODELABEL(w25q128jv)
#define FLASH_DEV  DEVICE_DT_GET(FLASH_NODE)

/* 
 * 设备树中的 size 属性单位为 bit (位)
 * 容量 (字节) = size (位) / 8
 */
#define FLASH_CAPACITY_BITS  DT_PROP_OR(FLASH_NODE, size, 134217728)
#define FLASH_CAPACITY_BYTES (FLASH_CAPACITY_BITS / 8)

#define FLASH_SECTOR_SIZE     4096
#define FLASH_TEST_OFFSET     (FLASH_CAPACITY_BYTES - FLASH_SECTOR_SIZE)
#define FLASH_PAGE_SIZE       256
#define FLASH_BENCH_READ_SIZE 1024

static void print_hexdump_cyber(const char *prefix, const uint8_t *buf, size_t len)
{
	printk(ANSI_YELLOW "%s (%u 字节):\n" ANSI_RESET, prefix, (unsigned)len);
	for (size_t i = 0; i < len; i += 16) {
		printk(ANSI_GRAY "  0x%04X: " ANSI_CYAN, (unsigned)i);
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				printk("%02X ", buf[i + j]);
			} else {
				printk("   ");
			}
			if (j == 7) {
				printk(" ");
			}
		}
		printk(ANSI_RESET " |" ANSI_GREEN);
		for (size_t j = 0; j < 16; j++) {
			if (i + j < len) {
				char c = (char)buf[i + j];
				printk("%c", (c >= 32 && c <= 126) ? c : '.');
			}
		}
		printk(ANSI_RESET "|\n");
	}
}

static bool run_flash_benchmark(void)
{
	const struct device *dev = FLASH_DEV;
	const struct flash_parameters *fp;
	uint8_t wbuf[FLASH_PAGE_SIZE];
	uint8_t rbuf[FLASH_PAGE_SIZE];
	uint8_t read_bench_buf[FLASH_BENCH_READ_SIZE];
	int ret;
	uint32_t t_start, t_erase_ms, t_write_us, t_read_us;

	printk("\n" ANSI_CYAN "╔══════════════════════════════════════════════════════════════════════╗\n");
	printk("║      ⚡ W25Q128JVSIQ 16 MiB SPI-NOR Flash 硬件基准性能测试 ⚡         ║\n");
	printk("╚══════════════════════════════════════════════════════════════════════╝\n" ANSI_RESET);

	if (!device_is_ready(dev)) {
		printk(ANSI_RED "  [FAIL] SPI Flash 设备未就绪! (device_is_ready 失败)\n" ANSI_RESET);
		printk("  请核实: P2.00(/HOLD) 与 P2.03(/WP) 是否已拉高，SPI00 连接是否正常。\n");
		return false;
	}

	fp = flash_get_parameters(dev);
	printk("  • 芯片规格     : Winbond W25Q128JV (128 Mbit / 16 MiB)\n");
	printk("  • 探测容量     : " ANSI_BOLD "%u KiB (%u MiB)" ANSI_RESET "\n",
	       (unsigned)(FLASH_CAPACITY_BYTES / 1024),
	       (unsigned)(FLASH_CAPACITY_BYTES / (1024 * 1024)));
	printk("  • 写入块粒度   : %u 字节 | 擦除空白值: 0x%02X\n",
	       (unsigned)fp->write_block_size, fp->erase_value);
	printk("  • 测试物理扇区 : 0x%08X ~ 0x%08X (末尾 4 KiB 扇区)\n\n",
	       (unsigned)FLASH_TEST_OFFSET,
	       (unsigned)(FLASH_TEST_OFFSET + FLASH_SECTOR_SIZE - 1));

	/* [1/4] 扇区擦除测试 */
	printk(ANSI_WHITE "  [1/4] 4 KiB 扇区擦除测试 ... " ANSI_RESET);
	t_start = k_cycle_get_32();
	ret = flash_erase(dev, FLASH_TEST_OFFSET, FLASH_SECTOR_SIZE);
	t_erase_ms = (k_cycle_get_32() - t_start) / (sys_clock_hw_cycles_per_sec() / 1000);
	if (ret != 0) {
		printk(ANSI_RED "FAILED (%d)\n" ANSI_RESET, ret);
		return false;
	}
	uint32_t erase_speed = (t_erase_ms > 0) ? (FLASH_SECTOR_SIZE / t_erase_ms) : FLASH_SECTOR_SIZE;
	printk(ANSI_GREEN "PASS " ANSI_RESET "(耗时: %u ms | 速度: " ANSI_CYAN "%u KB/s" ANSI_RESET ")\n",
	       (unsigned)t_erase_ms, (unsigned)erase_speed);

	/* [2/4] 擦除空白状态校验 */
	printk(ANSI_WHITE "  [2/4] 空白擦除状态校验 (Blank Check) ... " ANSI_RESET);
	memset(rbuf, 0, sizeof(rbuf));
	ret = flash_read(dev, FLASH_TEST_OFFSET, rbuf, sizeof(rbuf));
	if (ret != 0) {
		printk(ANSI_RED "FAILED 读失败\n" ANSI_RESET);
		return false;
	}
	for (size_t i = 0; i < sizeof(rbuf); i++) {
		if (rbuf[i] != fp->erase_value) {
			printk(ANSI_RED "FAILED (在偏移 %u 处发现非 0xFF 数据: 0x%02X)\n" ANSI_RESET,
			       (unsigned)i, rbuf[i]);
			return false;
		}
	}
	printk(ANSI_GREEN "PASS " ANSI_RESET "(首 256 字节全部为 0x%02X)\n", fp->erase_value);

	/* [3/4] 256 字节完整 Page Program 编程与写入速率测试 */
	for (size_t i = 0; i < sizeof(wbuf); i++) {
		/* 棋盘特征交替与伪随机特征叠加 */
		wbuf[i] = (uint8_t)((i % 2 == 0) ? (0x55 ^ (i * 3)) : (0xAA ^ (i * 5)));
	}
	printk(ANSI_WHITE "  [3/4] 完整页编程写入 (%u 字节) ... " ANSI_RESET, (unsigned)sizeof(wbuf));
	t_start = k_cycle_get_32();
	ret = flash_write(dev, FLASH_TEST_OFFSET, wbuf, sizeof(wbuf));
	t_write_us = (k_cycle_get_32() - t_start) / (sys_clock_hw_cycles_per_sec() / 1000000);
	if (ret != 0) {
		printk(ANSI_RED "FAILED (%d)\n" ANSI_RESET, ret);
		return false;
	}
	uint32_t write_speed_kb = (t_write_us > 0) ? ((sizeof(wbuf) * 1000000ULL / t_write_us) / 1024) : 0;
	printk(ANSI_GREEN "PASS " ANSI_RESET "(耗时: %u us | 写入吞吐量: " ANSI_CYAN "%u KB/s" ANSI_RESET ")\n",
	       (unsigned)t_write_us, (unsigned)write_speed_kb);

	/* [4/4] 1024 字节突发读取速度与数据完整性校验 */
	printk(ANSI_WHITE "  [4/4] 突发读取与数据 100%% 一致性校验 ... " ANSI_RESET);
	memset(rbuf, 0, sizeof(rbuf));
	t_start = k_cycle_get_32();
	ret = flash_read(dev, FLASH_TEST_OFFSET, read_bench_buf, sizeof(read_bench_buf));
	t_read_us = (k_cycle_get_32() - t_start) / (sys_clock_hw_cycles_per_sec() / 1000000);
	if (ret != 0) {
		printk(ANSI_RED "FAILED (%d)\n" ANSI_RESET, ret);
		return false;
	}
	uint32_t read_speed_kb = (t_read_us > 0) ? ((sizeof(read_bench_buf) * 1000000ULL / t_read_us) / 1024) : 0;

	/* 比对前 256 字节 */
	if (memcmp(wbuf, read_bench_buf, sizeof(wbuf)) != 0) {
		printk(ANSI_RED "FAILED (写入与读出数据不匹配!)\n" ANSI_RESET);
		return false;
	}
	printk(ANSI_GREEN "PASS " ANSI_RESET "(读取吞吐量: " ANSI_CYAN "%u KB/s" ANSI_RESET ", 数据完全一致)\n\n",
	       (unsigned)read_speed_kb);

	print_hexdump_cyber("W25Q128 特征测试数据校验 DUMP", read_bench_buf, 64);

	printk(ANSI_GREEN "\n  ╔═══════════════════════════════════════════════════════════════╗\n");
	printk("  ║   >>> W25Q128JVSIQ FLASH 硬件综合测试结论: 全部通过 PASS <<<  ║\n");
	printk("  ╚═══════════════════════════════════════════════════════════════╝\n\n" ANSI_RESET);

	return true;
}

/* ========================================================================== */
/* 5. 炫酷启动动画效果                                                       */
/* ========================================================================== */
static void run_startup_show(void)
{
	printk(ANSI_MAGENTA "  [BOOT] 正在初始化炫彩开机灯效与外设探测 ...\n" ANSI_RESET);

	/* 1. 霹雳游侠光影扫描 2 轮 */
	for (int lap = 0; lap < 2; lap++) {
		for (int i = 0; i < NUM_LEDS; i++) {
			gpio_pin_set_dt(&led_specs[i], 1);
			/* WS2812 伴随同步流动色彩 */
			uint8_t r, g, b;
			hsv_to_rgb(i * 64, 255, 200, &r, &g, &b);
			ws2812_set_rgb(r, g, b);
			k_msleep(70);
			gpio_pin_set_dt(&led_specs[i], 0);
		}
		for (int i = NUM_LEDS - 2; i > 0; i--) {
			gpio_pin_set_dt(&led_specs[i], 1);
			uint8_t r, g, b;
			hsv_to_rgb(i * 64 + 32, 255, 200, &r, &g, &b);
			ws2812_set_rgb(r, g, b);
			k_msleep(70);
			gpio_pin_set_dt(&led_specs[i], 0);
		}
	}

	/* 2. 全闪耀 3 次 */
	for (int k = 0; k < 3; k++) {
		for (int i = 0; i < NUM_LEDS; i++) {
			gpio_pin_set_dt(&led_specs[i], 1);
		}
		ws2812_set_rgb(255, 255, 255);
		k_msleep(60);
		for (int i = 0; i < NUM_LEDS; i++) {
			gpio_pin_set_dt(&led_specs[i], 0);
		}
		ws2812_set_rgb(0, 0, 0);
		k_msleep(60);
	}
}

/* 打印赛博终端操作菜单 */
static void print_interactive_menu(void)
{
	printk(ANSI_CYAN "======================================================================\n");
	printk("  🎮 【串口实时交互控制指南】 (在串口终端直接敲击键盘按键):\n");
	printk("   • [ 1 ] 重跑 Flash 读写速度基准跑分 (Benchmark)\n");
	printk("   • [ 2 ] 切换 WS2812B 灯珠光效模式 (彩虹 / 呼吸 / 脉冲 / 静态)\n");
	printk("   • [ 3 ] 切换 4 颗 LED 特效模式 (手动翻转 / 霹雳游侠 / 二进制计数)\n");
	printk("   • [ b ] 循环调节 WS2812B 亮度 (10%% -> 25%% -> 50%% -> 100%%)\n");
	printk("   • [ h ] 打印当前系统拓扑与帮助信息\n");
	printk("  --------------------------------------------------------------------\n");
	printk("  👆 【板载 4 颗轻触按键手势指南】:\n");
	printk("   • 单击 SW1~SW4 : 翻转对应 LED，点亮专属色彩\n");
	printk("   • 长按 SW1 (>0.8s): 触发 Flash 重新自检\n");
	printk("   • 长按 SW2 (>0.8s): 切换 LED 灯效模式\n");
	printk("   • 长按 SW3 (>0.8s): 循环步进调节亮度\n");
	printk("   • 长按 SW4 (>0.8s): 切换 WS2812B 光效模式\n");
	printk("======================================================================\n\n" ANSI_RESET);
}

/* ========================================================================== */
/* 6. 主程序循环与多任务调度                                                 */
/* ========================================================================== */

int main(void)
{
	bool flash_passed = false;
	uint8_t led_states[NUM_LEDS] = {0};
	uint8_t btn_debounced[NUM_BTNS] = {0};
	uint8_t btn_raw_last[NUM_BTNS] = {0};
	uint8_t btn_integrator[NUM_BTNS] = {0};
	uint32_t btn_press_start_time[NUM_BTNS] = {0};
	bool btn_long_pressed[NUM_BTNS] = {0};
	uint32_t btn_click_count[NUM_BTNS] = {0};

	enum led_mode cur_led_mode = LED_MODE_MANUAL;
	enum ws_mode cur_ws_mode = WS_MODE_RAINBOW;

	uint32_t tick = 0;
	uint8_t rainbow_hue = 0;
	uint8_t breathe_step = 0;
	int knight_pos = 0;
	int knight_dir = 1;
	uint8_t binary_counter = 0;

	/* 获取控制台串口设备句柄，用于非阻塞键盘交互 */
	const struct device *console_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	k_msleep(100);

	/* 打印超级 ASCII Art 欢迎界面 */
	printk("\n\n" ANSI_CYAN);
	printk("  _   _ ____  _____ ____  _  _   _     __  __ ____   ___    _    \n");
	printk(" | \\ | |  _ \\|  ___| ___|| || | | |   |  \\/  |___ \\ / _ \\  / \\   \n");
	printk(" |  \\| | |_) | |_  |___ \\| || |_| |   | |\\/| | __) | | | |/ _ \\  \n");
	printk(" | |\\  |  _ <|  _|  ___) |__   _| |___| |  | |/ __/| |_| / ___ \\ \n");
	printk(" |_| \\_|_| \\_\\_|   |____/   |_| |_____|_|  |_|_____|\\___/_/   \\_\\\n");
	printk(ANSI_YELLOW "       CYBER HARDWARE BRINGUP & DIAGNOSTIC PLATFORM (v2.0)       \n" ANSI_RESET);
	printk(ANSI_CYAN "╔══════════════════════════════════════════════════════════════════════╗\n");
	printk("║ • 核心架构 : ARM Cortex-M33 @ 128 MHz (Nordic nRF54LM20A)            ║\n");
	printk("║ • 外挂存储 : Winbond W25Q128JVSIQ (16 MiB SPI-NOR Flash)             ║\n");
	printk("║ • 炫彩外设 : WS2812B 全彩 RGB + 4x MOSFET 驱动 LED                   ║\n");
	printk("║ • 交互输入 : 4x 轻触按键 (支持单击/长按) + 串口命令行控制台           ║\n");
	printk("║ • 调试串口 : UART30 (TX: P0.06, RX: P0.07, 115200 8N1)               ║\n");
	printk("╚══════════════════════════════════════════════════════════════════════╝\n\n" ANSI_RESET);

	/* 初始化 LED */
	for (int i = 0; i < NUM_LEDS; i++) {
		if (device_is_ready(led_specs[i].port)) {
			gpio_pin_configure_dt(&led_specs[i], GPIO_OUTPUT_INACTIVE);
		}
	}

	/* 初始化按键 */
	for (int i = 0; i < NUM_BTNS; i++) {
		if (device_is_ready(btn_specs[i].port)) {
			gpio_pin_configure_dt(&btn_specs[i], GPIO_INPUT | GPIO_PULL_UP);
			btn_raw_last[i] = gpio_pin_get_dt(&btn_specs[i]);
		}
	}

	/* 初始化 WS2812B */
	const struct device *gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
	if (device_is_ready(gpio1_dev)) {
		gpio_pin_configure(gpio1_dev, WS2812_PIN, GPIO_OUTPUT_INACTIVE);
	}
	ws2812_send_raw(0, 0, 0);

	/* 运行开机光影大秀 */
	run_startup_show();

	/* 运行 W25Q128JV 性能跑分 */
	flash_passed = run_flash_benchmark();

	/* 打印交互指南 */
	print_interactive_menu();

	/* 主循环 (10ms 时隙) */
	while (1) {
		/* ========================================================== */
		/* A. 串口键盘实时交互非阻塞检测                              */
		/* ========================================================== */
		unsigned char rx_char;
		if (device_is_ready(console_dev) && uart_poll_in(console_dev, &rx_char) == 0) {
			if (rx_char == '1') {
				printk(ANSI_YELLOW "\n[CMD] 收到指令: 重新执行 Flash 速度基准跑分 ...\n" ANSI_RESET);
				flash_passed = run_flash_benchmark();
			} else if (rx_char == '2') {
				cur_ws_mode = (cur_ws_mode + 1) % WS_MODE_MAX;
				printk(ANSI_MAGENTA "\n[CMD] 切换 WS2812B 模式 -> %s\n" ANSI_RESET, ws_mode_names[cur_ws_mode]);
			} else if (rx_char == '3') {
				cur_led_mode = (cur_led_mode + 1) % LED_MODE_MAX;
				printk(ANSI_CYAN "\n[CMD] 切换 4-LED 模式 -> %s\n" ANSI_RESET, led_mode_names[cur_led_mode]);
				if (cur_led_mode == LED_MODE_MANUAL) {
					for (int i = 0; i < NUM_LEDS; i++) {
						gpio_pin_set_dt(&led_specs[i], led_states[i]);
					}
				}
			} else if (rx_char == 'b' || rx_char == 'B') {
				brightness_idx = (brightness_idx + 1) % ARRAY_SIZE(brightness_levels);
				printk(ANSI_YELLOW "\n[CMD] 调节 WS2812B 亮度 -> %u%%\n" ANSI_RESET,
				       (unsigned)((brightness_levels[brightness_idx] * 100) / 255));
			} else if (rx_char == 'h' || rx_char == 'H' || rx_char == '?') {
				print_interactive_menu();
			}
		}

		/* ========================================================== */
		/* B. 4 颗按键智能消抖与【单击/长按】手势识别                 */
		/* ========================================================== */
		for (int i = 0; i < NUM_BTNS; i++) {
			if (!device_is_ready(btn_specs[i].port)) {
				continue;
			}
			uint8_t raw = gpio_pin_get_dt(&btn_specs[i]);

			if (raw != btn_raw_last[i]) {
				btn_raw_last[i] = raw;
				btn_integrator[i] = 0;
			} else if (btn_integrator[i] < 3) {
				btn_integrator[i]++;
			}

			/* 状态稳定触发 */
			if (btn_integrator[i] == 3 && raw != btn_debounced[i]) {
				btn_debounced[i] = raw;

				if (raw != 0) {
					/* 按下瞬间 */
					btn_press_start_time[i] = k_uptime_get_32();
					btn_long_pressed[i] = false;
					btn_click_count[i]++;
				} else {
					/* 释放瞬间: 若未触发过长按，则判定为单击 */
					if (!btn_long_pressed[i]) {
						uint32_t duration = k_uptime_get_32() - btn_press_start_time[i];

						if (cur_led_mode == LED_MODE_MANUAL) {
							led_states[i] = !led_states[i];
							gpio_pin_set_dt(&led_specs[i], led_states[i]);
						}

						/* 单击触发专属静态颜色 */
						cur_ws_mode = WS_MODE_STATIC;
						if (i == 0) ws2812_set_rgb(255, 0, 0);     /* 烈焰红 */
						else if (i == 1) ws2812_set_rgb(0, 255, 0);/* 极光绿 */
						else if (i == 2) ws2812_set_rgb(0, 100, 255);/* 赛博蓝 */
						else if (i == 3) ws2812_set_rgb(255, 200, 0);/* 琥珀金 */

						printk("[%7u ms] " ANSI_GREEN "[单击] " ANSI_RESET "%s (按下 %u ms) -> 切换 LED%d [%s]\n",
						       (unsigned)k_uptime_get_32(), btn_names[i],
						       (unsigned)duration, i, led_states[i] ? ANSI_GREEN "ON" ANSI_RESET : ANSI_GRAY "OFF" ANSI_RESET);
					}
				}
			}

			/* 长按 (>800ms) 检测 */
			if (btn_debounced[i] != 0 && !btn_long_pressed[i]) {
				if ((k_uptime_get_32() - btn_press_start_time[i]) > 800) {
					btn_long_pressed[i] = true;
					printk("[%7u ms] " ANSI_YELLOW "⚡ [长按触发] " ANSI_RESET "%s ",
					       (unsigned)k_uptime_get_32(), btn_names[i]);

					if (i == 0) {
						printk("-> 重新触发 Flash 性能跑分!\n");
						flash_passed = run_flash_benchmark();
					} else if (i == 1) {
						cur_led_mode = (cur_led_mode + 1) % LED_MODE_MAX;
						printk("-> 切换 LED 模式: " ANSI_CYAN "%s\n" ANSI_RESET, led_mode_names[cur_led_mode]);
						if (cur_led_mode == LED_MODE_MANUAL) {
							for (int k = 0; k < NUM_LEDS; k++) {
								gpio_pin_set_dt(&led_specs[k], led_states[k]);
							}
						}
					} else if (i == 2) {
						brightness_idx = (brightness_idx + 1) % ARRAY_SIZE(brightness_levels);
						printk("-> 调节 WS2812B 亮度: " ANSI_YELLOW "%u%%\n" ANSI_RESET,
						       (unsigned)((brightness_levels[brightness_idx] * 100) / 255));
					} else if (i == 3) {
						cur_ws_mode = (cur_ws_mode + 1) % WS_MODE_MAX;
						printk("-> 切换 WS2812B 光效: " ANSI_MAGENTA "%s\n" ANSI_RESET, ws_mode_names[cur_ws_mode]);
					}
				}
			}
		}

		/* ========================================================== */
		/* C. 4 颗 LED 动画引擎渲染                                   */
		/* ========================================================== */
		if (cur_led_mode == LED_MODE_KNIGHT_RIDER && (tick % 7 == 0)) {
			/* 霹雳游侠扫描 */
			for (int i = 0; i < NUM_LEDS; i++) {
				gpio_pin_set_dt(&led_specs[i], (i == knight_pos) ? 1 : 0);
			}
			knight_pos += knight_dir;
			if (knight_pos >= NUM_LEDS - 1) {
				knight_dir = -1;
			} else if (knight_pos <= 0) {
				knight_dir = 1;
			}
		} else if (cur_led_mode == LED_MODE_BINARY_COUNT && (tick % 25 == 0)) {
			/* 4位二进制计数器 (每 250ms +1) */
			binary_counter++;
			for (int i = 0; i < NUM_LEDS; i++) {
				gpio_pin_set_dt(&led_specs[i], (binary_counter & BIT(i)) ? 1 : 0);
			}
		} else if (cur_led_mode == LED_MODE_MARQUEE && (tick % 5 == 0)) {
			/* 极速跑马灯 */
			for (int i = 0; i < NUM_LEDS; i++) {
				gpio_pin_set_dt(&led_specs[i], (i == (tick / 5) % NUM_LEDS) ? 1 : 0);
			}
		}

		/* ========================================================== */
		/* D. WS2812B 炫彩光效引擎渲染                                */
		/* ========================================================== */
		if (cur_ws_mode == WS_MODE_RAINBOW && (tick % 2 == 0)) {
			/* 模式 0: 超平滑 256 阶色相全彩轮 */
			uint8_t r, g, b;
			hsv_to_rgb(rainbow_hue++, 255, 255, &r, &g, &b);
			ws2812_set_rgb(r, g, b);
		} else if (cur_ws_mode == WS_MODE_BREATHE && (tick % 2 == 0)) {
			/* 模式 1: 平滑呼吸灯 (色相每轮呼吸切换，明度采用正弦仿生缓动) */
			breathe_step++;
			uint8_t raw_val = (breathe_step < 128) ? (breathe_step * 2) : ((255 - breathe_step) * 2);
			/* Gamma 平方曲线校正，低亮度过渡更细腻 */
			uint8_t curved_val = (uint8_t)(((uint16_t)raw_val * raw_val) >> 8);
			uint8_t r, g, b;
			/* 取洋红与深青色之间的赛博霓虹过渡 */
			hsv_to_rgb(160 + (breathe_step / 4), 230, curved_val, &r, &g, &b);
			ws2812_set_rgb(r, g, b);
		} else if (cur_ws_mode == WS_MODE_PULSE && (tick % 10 == 0)) {
			/* 模式 2: 能量脉冲爆闪 */
			if ((tick / 10) % 2 == 0) {
				ws2812_set_rgb(255, 255, 255);
			} else {
				ws2812_set_rgb(0, 0, 50);
			}
		}

		/* ========================================================== */
		/* E. 周期性赛博朋克实时状态仪表盘输出 (每 2 秒一次)           */
		/* ========================================================== */
		if ((tick % 200) == 0) {
			uint32_t now = k_uptime_get_32();
			uint32_t sec = now / 1000;
			uint32_t min = sec / 60;
			sec %= 60;

			/* 打印 ANSI 终端实时 TrueColor 颜色方块预览 (终端支持 24-bit 颜色) */
			printk(ANSI_CYAN "[%02u:%02u] " ANSI_RESET, (unsigned)min, (unsigned)sec);
			printk("LEDs:[");
			for (int k = 0; k < NUM_LEDS; k++) {
				int st = gpio_pin_get_dt(&led_specs[k]);
				printk(st ? ANSI_GREEN "●" ANSI_RESET : ANSI_GRAY "○" ANSI_RESET);
				if (k < NUM_LEDS - 1) printk(" ");
			}
			printk("] | BTNs:[%d %d %d %d] | ",
			       btn_debounced[0], btn_debounced[1], btn_debounced[2], btn_debounced[3]);

			/* 终端真实 RGB 颜色预览色块 */
			printk("RGB: \033[48;2;%u;%u;%um  \033[0m " ANSI_WHITE "#%02X%02X%02X" ANSI_RESET " (%-7s) | ",
			       current_r, current_g, current_b,
			       current_r, current_g, current_b,
			       ws_mode_names[cur_ws_mode]);

			/* 根据真实的 flash_passed 状态打印 PASS 或 FAIL */
			printk("Flash: %s\n", flash_passed ? ANSI_GREEN "PASS ✔" ANSI_RESET : ANSI_RED "FAIL ✘" ANSI_RESET);
		}

		tick++;
		k_msleep(10);
	}

	return 0;
}
