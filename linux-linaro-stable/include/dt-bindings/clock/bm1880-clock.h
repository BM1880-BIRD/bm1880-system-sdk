#ifndef __BM1880_CLOCK__
#define __BM1880_CLOCK__

//PLL ID
#define MPLL_CLK			0
#define SPLL_CLK			1
#define FPLL_CLK			2
#define DPLL_CLK			3

//clock mode
#define NORMAL_MODE			0
#define FAST_MODE			1
#define SAFE_MODE			2
#define BYPASS_MODE			3

/* gate clocks */
#define GATE_CLK_NF			0
#define GATE_CLK_APB_NF			1
#define GATE_CLK_AXI5_NF		2
#define GATE_CLK_JPEG_AXI		3
#define GATE_CLK_APB_JPEG		4
#define GATE_CLK_APB_WDT		5
#define GATE_CLK_APB_I2C		6
#define GATE_CLK_SDMA_AUD		7
#define GATE_CLK_SDMA_AXI		8
#define GATE_CLK_AHB_SF			9
#define GATE_CLK_AXI1_MINER		10
#define GATE_CLK_GPIO_DB		11
#define GATE_CLK_APB_GPIO_INTR		12
#define GATE_CLK_APB_GPIO		13
#define GATE_CLK_AXI1_GDMA		14
#define GATE_CLK_AXI4_ETH1		15
#define GATE_CLK_500M_ETH1		16
#define GATE_CLK_AXI4_ETH0		17
#define GATE_CLK_500M_ETH0		18
#define GATE_CLK_100K_SD		19
#define GATE_CLK_SD			20
#define GATE_CLK_AXI5_SD		21
#define GATE_CLK_100K_EMMC		22
#define GATE_CLK_EMMC			23
#define GATE_CLK_AXI5_EMMC		24
#define GATE_CLK_APB_EFUSE		25
#define GATE_CLK_EFUSE			26
#define GATE_CLK_DDR_AXI_REG		27
#define GATE_CLK_AXI1_AXISRAM		28
#define GATE_CLK_AHB_ROM		29
#define GATE_CLK_50M_A53		30
#define GATE_CLK_A53			31
#define GATE_CLK_AXI6			32
#define GATE_CLK_AXI5			33
#define GATE_CLK_AXI4			34
#define GATE_CLK_AXI3			35
#define GATE_CLK_AXI2			36
#define GATE_CLK_AXI1			37
#define GATE_CLK_APB_VPP		38
#define GATE_CLK_VPP_AXI		39
#define GATE_CLK_VIDEO_AXI		40
#define GATE_CLK_APB_VIDEO		41
#define GATE_CLK_12M_USB		42
#define GATE_CLK_33K_USB		43
#define GATE_CLK_125M_USB		44
#define GATE_CLK_APB_USB		45
#define GATE_CLK_AXI4_USB		46
#define GATE_CLK_APB_I2S		47
#define GATE_CLK_APB_UART		48
#define GATE_CLK_UART_500M		49
#define GATE_CLK_TPU_AXI		50
#define GATE_CLK_APB_SPI		51
#define GATE_CLK_RV			52
#define GATE_CLK_APB_PWM		53

/* mux clocks */
#define MUX_CLK_AXI6			0
#define MUX_CLK_AXI1			1
#define MUX_CLK_RV			2
#define MUX_CLK_A53			3

/* divider clocks */
#define DIV_CLK_50M_A53			0
#define DIV_CLK_EFUSE			1
#define DIV_CLK_EMMC			2
#define DIV_CLK_100K_EMMC		3
#define DIV_CLK_SD			4
#define DIV_CLK_100K_SD			5
#define DIV_CLK_500M_ETH0		6
#define DIV_CLK_500M_ETH1		7
#define DIV_CLK_GPIO_DB			8
#define DIV_CLK_SDMA_AUD		9
#define DIV_CLK_JPEG_AXI		10
#define DIV_CLK_NF			11
#define DIV_0_CLK_RV			12
#define DIV_1_CLK_RV			13
#define DIV_CLK_TPU_AXI			14
#define DIV_CLK_UART_500M		15
#define DIV_CLK_125M_USB		16
#define DIV_CLK_33K_USB			17
#define DIV_CLK_12M_USB			18
#define DIV_CLK_VIDEO_AXI		19
#define DIV_CLK_VPP_AXI			20
#define DIV_0_CLK_AXI1			21
#define DIV_1_CLK_AXI1			22
#define DIV_CLK_AXI2			23
#define DIV_CLK_AXI3			24
#define DIV_CLK_AXI4			25
#define DIV_CLK_AXI5			26
#define DIV_0_CLK_AXI6			27
#define DIV_1_CLK_AXI6			28

//Clock Table ID
#define DIV_CLK_TABLE			0
#define MUX_CLK_TABLE			1

#endif
