#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of.h>
#include "pinctrl-utils.h"

struct bm_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl;
	void __iomem *virtbase;
};

struct bm_dev_mux {
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct pinctrl_state *acquire, *release;
};

struct bm_group {
	const char *name;
	const unsigned int *pins;
	const unsigned int num_pins;
};

static struct class *bm_class;


static const struct pinctrl_pin_desc bm_pins[] = {
	PINCTRL_PIN(0,   "MIO0"),
	PINCTRL_PIN(1,   "MIO1"),
	PINCTRL_PIN(2,   "MIO2"),
	PINCTRL_PIN(3,   "MIO3"),
	PINCTRL_PIN(4,   "MIO4"),
	PINCTRL_PIN(5,   "MIO5"),
	PINCTRL_PIN(6,   "MIO6"),
	PINCTRL_PIN(7,   "MIO7"),
	PINCTRL_PIN(8,   "MIO8"),
	PINCTRL_PIN(9,   "MIO9"),
	PINCTRL_PIN(10,   "MIO10"),
	PINCTRL_PIN(11,   "MIO11"),
	PINCTRL_PIN(12,   "MIO12"),
	PINCTRL_PIN(13,   "MIO13"),
	PINCTRL_PIN(14,   "MIO14"),
	PINCTRL_PIN(15,   "MIO15"),
	PINCTRL_PIN(16,   "MIO16"),
	PINCTRL_PIN(17,   "MIO17"),
	PINCTRL_PIN(18,   "MIO18"),
	PINCTRL_PIN(19,   "MIO19"),
	PINCTRL_PIN(20,   "MIO20"),
	PINCTRL_PIN(21,   "MIO21"),
	PINCTRL_PIN(22,   "MIO22"),
	PINCTRL_PIN(23,   "MIO23"),
	PINCTRL_PIN(24,   "MIO24"),
	PINCTRL_PIN(25,   "MIO25"),
	PINCTRL_PIN(26,   "MIO26"),
	PINCTRL_PIN(27,   "MIO27"),
	PINCTRL_PIN(28,   "MIO28"),
	PINCTRL_PIN(29,   "MIO29"),
	PINCTRL_PIN(30,   "MIO30"),
	PINCTRL_PIN(31,   "MIO31"),
	PINCTRL_PIN(32,   "MIO32"),
	PINCTRL_PIN(33,   "MIO33"),
	PINCTRL_PIN(34,   "MIO34"),
	PINCTRL_PIN(35,   "MIO35"),
	PINCTRL_PIN(36,   "MIO36"),
	PINCTRL_PIN(37,   "MIO37"),
	PINCTRL_PIN(38,   "MIO38"),
	PINCTRL_PIN(39,   "MIO39"),
	PINCTRL_PIN(40,   "MIO40"),
	PINCTRL_PIN(41,   "MIO41"),
	PINCTRL_PIN(42,   "MIO42"),
	PINCTRL_PIN(43,   "MIO43"),
	PINCTRL_PIN(44,   "MIO44"),
	PINCTRL_PIN(45,   "MIO45"),
	PINCTRL_PIN(46,   "MIO46"),
	PINCTRL_PIN(47,   "MIO47"),
	PINCTRL_PIN(48,   "MIO48"),
	PINCTRL_PIN(49,   "MIO49"),
	PINCTRL_PIN(50,   "MIO50"),
	PINCTRL_PIN(51,   "MIO51"),
	PINCTRL_PIN(52,   "MIO52"),
	PINCTRL_PIN(53,   "MIO53"),
	PINCTRL_PIN(54,   "MIO54"),
	PINCTRL_PIN(55,   "MIO55"),
	PINCTRL_PIN(56,   "MIO56"),
	PINCTRL_PIN(57,   "MIO57"),
	PINCTRL_PIN(58,   "MIO58"),
	PINCTRL_PIN(59,   "MIO59"),
	PINCTRL_PIN(60,   "MIO60"),
	PINCTRL_PIN(61,   "MIO61"),
	PINCTRL_PIN(62,   "MIO62"),
	PINCTRL_PIN(63,   "MIO63"),
	PINCTRL_PIN(64,   "MIO64"),
	PINCTRL_PIN(65,   "MIO65"),
	PINCTRL_PIN(66,   "MIO66"),
	PINCTRL_PIN(67,   "MIO67"),
	PINCTRL_PIN(68,   "MIO68"),
	PINCTRL_PIN(69,   "MIO69"),
	PINCTRL_PIN(70,   "MIO70"),
	PINCTRL_PIN(71,   "MIO71"),
	PINCTRL_PIN(72,   "MIO72"),
	PINCTRL_PIN(73,   "MIO73"),
	PINCTRL_PIN(74,   "MIO74"),
	PINCTRL_PIN(75,   "MIO75"),
	PINCTRL_PIN(76,   "MIO76"),
	PINCTRL_PIN(77,   "MIO77"),
	PINCTRL_PIN(78,   "MIO78"),
	PINCTRL_PIN(79,   "MIO79"),
	PINCTRL_PIN(80,   "MIO80"),
	PINCTRL_PIN(81,   "MIO81"),
	PINCTRL_PIN(82,   "MIO82"),
	PINCTRL_PIN(83,   "MIO83"),
	PINCTRL_PIN(84,   "MIO84"),
	PINCTRL_PIN(85,   "MIO85"),
	PINCTRL_PIN(86,   "MIO86"),
	PINCTRL_PIN(87,   "MIO87"),
	PINCTRL_PIN(88,   "MIO88"),
	PINCTRL_PIN(89,   "MIO89"),
	PINCTRL_PIN(90,   "MIO90"),
	PINCTRL_PIN(91,   "MIO91"),
	PINCTRL_PIN(92,   "MIO92"),
	PINCTRL_PIN(93,   "MIO93"),
	PINCTRL_PIN(94,   "MIO94"),
	PINCTRL_PIN(95,   "MIO95"),
	PINCTRL_PIN(96,   "MIO96"),
	PINCTRL_PIN(97,   "MIO97"),
	PINCTRL_PIN(98,   "MIO98"),
	PINCTRL_PIN(99,   "MIO99"),
	PINCTRL_PIN(100,   "MIO100"),
	PINCTRL_PIN(101,   "MIO101"),
	PINCTRL_PIN(102,   "MIO102"),
	PINCTRL_PIN(103,   "MIO103"),
	PINCTRL_PIN(104,   "MIO104"),
	PINCTRL_PIN(105,   "MIO105"),
	PINCTRL_PIN(106,   "MIO106"),
	PINCTRL_PIN(107,   "MIO107"),
	PINCTRL_PIN(108,   "MIO108"),
	PINCTRL_PIN(109,   "MIO109"),
	PINCTRL_PIN(110,   "MIO110"),
	PINCTRL_PIN(111,   "MIO111"),
};

static const unsigned int nand_pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const unsigned int spi_pins[] = {0, 1, 8, 10, 11, 12, 13};
static const unsigned int emmc_pins[] = {2, 3, 4, 5, 6, 7, 9, 14, 15, 16};
static const unsigned int sdio_pins[] = {17, 18, 19, 20, 21, 22, 23, 24, 25, 26};
static const unsigned int eth0_pins[] = {27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42};
static const unsigned int pwm0_pins[] = {29};
static const unsigned int pwm1_pins[] = {30};
static const unsigned int pwm2_pins[] = {34};
static const unsigned int pwm3_pins[] = {35};
static const unsigned int pwm4_pins[] = {43};
static const unsigned int pwm5_pins[] = {44};
static const unsigned int pwm6_pins[] = {45};
static const unsigned int pwm7_pins[] = {46};
static const unsigned int pwm8_pins[] = {47};
static const unsigned int pwm9_pins[] = {48};
static const unsigned int pwm10_pins[] = {49};
static const unsigned int pwm11_pins[] = {50};
static const unsigned int pwm12_pins[] = {51};
static const unsigned int pwm13_pins[] = {52};
static const unsigned int pwm14_pins[] = {53};
static const unsigned int pwm15_pins[] = {54};
static const unsigned int pwm16_pins[] = {55};
static const unsigned int pwm17_pins[] = {56};
static const unsigned int pwm18_pins[] = {57};
static const unsigned int pwm19_pins[] = {58};
static const unsigned int pwm20_pins[] = {59};
static const unsigned int pwm21_pins[] = {60};
static const unsigned int pwm22_pins[] = {61};
static const unsigned int pwm23_pins[] = {62};
static const unsigned int pwm24_pins[] = {97};
static const unsigned int pwm25_pins[] = {98};
static const unsigned int pwm26_pins[] = {99};
static const unsigned int pwm27_pins[] = {100};
static const unsigned int pwm28_pins[] = {101};
static const unsigned int pwm29_pins[] = {102};
static const unsigned int pwm30_pins[] = {103};
static const unsigned int pwm31_pins[] = {104};
static const unsigned int pwm32_pins[] = {105};
static const unsigned int pwm33_pins[] = {106};
static const unsigned int pwm34_pins[] = {107};
static const unsigned int pwm35_pins[] = {108};
static const unsigned int pwm36_pins[] = {109};
static const unsigned int pwm37_pins[] = {110};
static const unsigned int i2c0_pins[] = {63, 64};
static const unsigned int i2c1_pins[] = {65, 66};
static const unsigned int i2c2_pins[] = {67, 68};
static const unsigned int i2c3_pins[] = {69, 70};
static const unsigned int i2c4_pins[] = {71, 72};
static const unsigned int uart0_pins[] = {73, 74};
static const unsigned int uart1_pins[] = {75, 76};
static const unsigned int uart2_pins[] = {77, 78};
static const unsigned int uart3_pins[] = {79, 80};
static const unsigned int uart4_pins[] = {81, 82};
static const unsigned int uart5_pins[] = {83, 84};
static const unsigned int uart6_pins[] = {85, 86};
static const unsigned int uart7_pins[] = {87, 88};
static const unsigned int uart8_pins[] = {89, 90};
static const unsigned int uart9_pins[] = {91, 92};
static const unsigned int uart10_pins[] = {93, 94};
static const unsigned int uart11_pins[] = {95, 96};
static const unsigned int gpio0_pins[] = {97};
static const unsigned int gpio1_pins[] = {98};
static const unsigned int gpio2_pins[] = {99};
static const unsigned int gpio3_pins[] = {100};
static const unsigned int gpio4_pins[] = {101};
static const unsigned int gpio5_pins[] = {102};
static const unsigned int gpio6_pins[] = {103};
static const unsigned int gpio7_pins[] = {104};
static const unsigned int gpio8_pins[] = {105};
static const unsigned int gpio9_pins[] = {106};
static const unsigned int gpio10_pins[] = {107};
static const unsigned int gpio11_pins[] = {108};
static const unsigned int gpio12_pins[] = {109};
static const unsigned int gpio13_pins[] = {110};
static const unsigned int gpio14_pins[] = {43};
static const unsigned int gpio15_pins[] = {44};
static const unsigned int gpio16_pins[] = {45};
static const unsigned int gpio17_pins[] = {46};
static const unsigned int gpio18_pins[] = {47};
static const unsigned int gpio19_pins[] = {48};
static const unsigned int gpio20_pins[] = {49};
static const unsigned int gpio21_pins[] = {50};
static const unsigned int gpio22_pins[] = {51};
static const unsigned int gpio23_pins[] = {52};
static const unsigned int gpio24_pins[] = {53};
static const unsigned int gpio25_pins[] = {54};
static const unsigned int gpio26_pins[] = {55};
static const unsigned int gpio27_pins[] = {56};
static const unsigned int gpio28_pins[] = {57};
static const unsigned int gpio29_pins[] = {58};
static const unsigned int gpio30_pins[] = {59};
static const unsigned int gpio31_pins[] = {60};
static const unsigned int gpio32_pins[] = {61};
static const unsigned int gpio33_pins[] = {62};
static const unsigned int gpio34_pins[] = {63};
static const unsigned int gpio35_pins[] = {64};
static const unsigned int gpio36_pins[] = {65};
static const unsigned int gpio37_pins[] = {66};
static const unsigned int gpio38_pins[] = {67};
static const unsigned int gpio39_pins[] = {68};
static const unsigned int gpio40_pins[] = {69};
static const unsigned int gpio41_pins[] = {70};
static const unsigned int gpio42_pins[] = {71};
static const unsigned int gpio43_pins[] = {72};
static const unsigned int gpio44_pins[] = {73};
static const unsigned int gpio45_pins[] = {74};
static const unsigned int gpio46_pins[] = {75};
static const unsigned int gpio47_pins[] = {76};
static const unsigned int gpio48_pins[] = {77};
static const unsigned int gpio49_pins[] = {78};
static const unsigned int gpio50_pins[] = {79};
static const unsigned int gpio51_pins[] = {80};
static const unsigned int gpio52_pins[] = {81};
static const unsigned int gpio53_pins[] = {82};
static const unsigned int gpio54_pins[] = {83};
static const unsigned int gpio55_pins[] = {84};
static const unsigned int gpio56_pins[] = {85};
static const unsigned int gpio57_pins[] = {86};
static const unsigned int gpio58_pins[] = {87};
static const unsigned int gpio59_pins[] = {88};
static const unsigned int gpio60_pins[] = {89};
static const unsigned int gpio61_pins[] = {90};
static const unsigned int gpio62_pins[] = {91};
static const unsigned int gpio63_pins[] = {92};
static const unsigned int gpio64_pins[] = {93};
static const unsigned int gpio65_pins[] = {94};
static const unsigned int gpio66_pins[] = {95};
static const unsigned int gpio67_pins[] = {96};
static const unsigned int eth1_pins[] = {43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};
static const unsigned int i2s0_pins[] = {87, 88, 89, 90, 91};
static const unsigned int i2s0_mclkin_pins[] = {97};
static const unsigned int i2s1_pins[] = {92, 93, 94, 95, 96};
static const unsigned int i2s1_mclkin_pins[] = {98};
static const unsigned int spi0_pins[] = {59, 60, 61, 62};
static const unsigned int null_pins[] = {};

static const struct bm_group bm_groups[] = {
	{"nand_grp", nand_pins, ARRAY_SIZE(nand_pins)},
	{"spi_grp", spi_pins, ARRAY_SIZE(spi_pins)},
	{"emmc_grp", emmc_pins, ARRAY_SIZE(emmc_pins)},
	{"sdio_grp", sdio_pins, ARRAY_SIZE(sdio_pins)},
	{"eth0_grp", eth0_pins, ARRAY_SIZE(eth0_pins)},
	{"pwm0_grp", pwm0_pins, ARRAY_SIZE(pwm0_pins)},
	{"pwm1_grp", pwm1_pins, ARRAY_SIZE(pwm1_pins)},
	{"pwm2_grp", pwm2_pins, ARRAY_SIZE(pwm2_pins)},
	{"pwm3_grp", pwm3_pins, ARRAY_SIZE(pwm3_pins)},
	{"pwm4_grp", pwm4_pins, ARRAY_SIZE(pwm4_pins)},
	{"pwm5_grp", pwm5_pins, ARRAY_SIZE(pwm5_pins)},
	{"pwm6_grp", pwm6_pins, ARRAY_SIZE(pwm6_pins)},
	{"pwm7_grp", pwm7_pins, ARRAY_SIZE(pwm7_pins)},
	{"pwm8_grp", pwm8_pins, ARRAY_SIZE(pwm8_pins)},
	{"pwm9_grp", pwm9_pins, ARRAY_SIZE(pwm9_pins)},
	{"pwm10_grp", pwm10_pins, ARRAY_SIZE(pwm10_pins)},
	{"pwm11_grp", pwm11_pins, ARRAY_SIZE(pwm11_pins)},
	{"pwm12_grp", pwm12_pins, ARRAY_SIZE(pwm12_pins)},
	{"pwm13_grp", pwm13_pins, ARRAY_SIZE(pwm13_pins)},
	{"pwm14_grp", pwm14_pins, ARRAY_SIZE(pwm14_pins)},
	{"pwm15_grp", pwm15_pins, ARRAY_SIZE(pwm15_pins)},
	{"pwm16_grp", pwm16_pins, ARRAY_SIZE(pwm16_pins)},
	{"pwm17_grp", pwm17_pins, ARRAY_SIZE(pwm17_pins)},
	{"pwm18_grp", pwm18_pins, ARRAY_SIZE(pwm18_pins)},
	{"pwm19_grp", pwm19_pins, ARRAY_SIZE(pwm19_pins)},
	{"pwm20_grp", pwm20_pins, ARRAY_SIZE(pwm20_pins)},
	{"pwm21_grp", pwm21_pins, ARRAY_SIZE(pwm21_pins)},
	{"pwm22_grp", pwm22_pins, ARRAY_SIZE(pwm22_pins)},
	{"pwm23_grp", pwm23_pins, ARRAY_SIZE(pwm23_pins)},
	{"pwm24_grp", pwm24_pins, ARRAY_SIZE(pwm24_pins)},
	{"pwm25_grp", pwm25_pins, ARRAY_SIZE(pwm25_pins)},
	{"pwm26_grp", pwm26_pins, ARRAY_SIZE(pwm26_pins)},
	{"pwm27_grp", pwm27_pins, ARRAY_SIZE(pwm27_pins)},
	{"pwm28_grp", pwm28_pins, ARRAY_SIZE(pwm28_pins)},
	{"pwm29_grp", pwm29_pins, ARRAY_SIZE(pwm29_pins)},
	{"pwm30_grp", pwm30_pins, ARRAY_SIZE(pwm30_pins)},
	{"pwm31_grp", pwm31_pins, ARRAY_SIZE(pwm31_pins)},
	{"pwm32_grp", pwm32_pins, ARRAY_SIZE(pwm32_pins)},
	{"pwm33_grp", pwm33_pins, ARRAY_SIZE(pwm33_pins)},
	{"pwm34_grp", pwm34_pins, ARRAY_SIZE(pwm34_pins)},
	{"pwm35_grp", pwm35_pins, ARRAY_SIZE(pwm35_pins)},
	{"pwm36_grp", pwm36_pins, ARRAY_SIZE(pwm36_pins)},
	{"pwm37_grp", pwm37_pins, ARRAY_SIZE(pwm37_pins)},
	{"i2c0_grp", i2c0_pins, ARRAY_SIZE(i2c0_pins)},
	{"i2c1_grp", i2c1_pins, ARRAY_SIZE(i2c1_pins)},
	{"i2c2_grp", i2c2_pins, ARRAY_SIZE(i2c2_pins)},
	{"i2c3_grp", i2c3_pins, ARRAY_SIZE(i2c3_pins)},
	{"i2c4_grp", i2c4_pins, ARRAY_SIZE(i2c4_pins)},
	{"uart0_grp", uart0_pins, ARRAY_SIZE(uart0_pins)},
	{"uart1_grp", uart1_pins, ARRAY_SIZE(uart1_pins)},
	{"uart2_grp", uart2_pins, ARRAY_SIZE(uart2_pins)},
	{"uart3_grp", uart3_pins, ARRAY_SIZE(uart3_pins)},
	{"uart4_grp", uart4_pins, ARRAY_SIZE(uart4_pins)},
	{"uart5_grp", uart5_pins, ARRAY_SIZE(uart5_pins)},
	{"uart6_grp", uart6_pins, ARRAY_SIZE(uart6_pins)},
	{"uart7_grp", uart7_pins, ARRAY_SIZE(uart7_pins)},
	{"uart8_grp", uart8_pins, ARRAY_SIZE(uart8_pins)},
	{"uart9_grp", uart9_pins, ARRAY_SIZE(uart9_pins)},
	{"uart10_grp", uart10_pins, ARRAY_SIZE(uart10_pins)},
	{"uart11_grp", uart11_pins, ARRAY_SIZE(uart11_pins)},
	{"gpio0_grp", gpio0_pins, ARRAY_SIZE(gpio0_pins)},
	{"gpio1_grp", gpio1_pins, ARRAY_SIZE(gpio1_pins)},
	{"gpio2_grp", gpio2_pins, ARRAY_SIZE(gpio2_pins)},
	{"gpio3_grp", gpio3_pins, ARRAY_SIZE(gpio3_pins)},
	{"gpio4_grp", gpio4_pins, ARRAY_SIZE(gpio4_pins)},
	{"gpio5_grp", gpio5_pins, ARRAY_SIZE(gpio5_pins)},
	{"gpio6_grp", gpio6_pins, ARRAY_SIZE(gpio6_pins)},
	{"gpio7_grp", gpio7_pins, ARRAY_SIZE(gpio7_pins)},
	{"gpio8_grp", gpio8_pins, ARRAY_SIZE(gpio8_pins)},
	{"gpio9_grp", gpio9_pins, ARRAY_SIZE(gpio9_pins)},
	{"gpio10_grp", gpio10_pins, ARRAY_SIZE(gpio10_pins)},
	{"gpio11_grp", gpio11_pins, ARRAY_SIZE(gpio11_pins)},
	{"gpio12_grp", gpio12_pins, ARRAY_SIZE(gpio12_pins)},
	{"gpio13_grp", gpio13_pins, ARRAY_SIZE(gpio13_pins)},
	{"gpio14_grp", gpio14_pins, ARRAY_SIZE(gpio14_pins)},
	{"gpio15_grp", gpio15_pins, ARRAY_SIZE(gpio15_pins)},
	{"gpio16_grp", gpio16_pins, ARRAY_SIZE(gpio16_pins)},
	{"gpio17_grp", gpio17_pins, ARRAY_SIZE(gpio17_pins)},
	{"gpio18_grp", gpio18_pins, ARRAY_SIZE(gpio18_pins)},
	{"gpio19_grp", gpio19_pins, ARRAY_SIZE(gpio19_pins)},
	{"gpio20_grp", gpio20_pins, ARRAY_SIZE(gpio20_pins)},
	{"gpio21_grp", gpio21_pins, ARRAY_SIZE(gpio21_pins)},
	{"gpio22_grp", gpio22_pins, ARRAY_SIZE(gpio22_pins)},
	{"gpio23_grp", gpio23_pins, ARRAY_SIZE(gpio23_pins)},
	{"gpio24_grp", gpio24_pins, ARRAY_SIZE(gpio24_pins)},
	{"gpio25_grp", gpio25_pins, ARRAY_SIZE(gpio25_pins)},
	{"gpio26_grp", gpio26_pins, ARRAY_SIZE(gpio26_pins)},
	{"gpio27_grp", gpio27_pins, ARRAY_SIZE(gpio27_pins)},
	{"gpio28_grp", gpio28_pins, ARRAY_SIZE(gpio28_pins)},
	{"gpio29_grp", gpio29_pins, ARRAY_SIZE(gpio29_pins)},
	{"gpio30_grp", gpio30_pins, ARRAY_SIZE(gpio30_pins)},
	{"gpio31_grp", gpio31_pins, ARRAY_SIZE(gpio31_pins)},
	{"gpio32_grp", gpio32_pins, ARRAY_SIZE(gpio32_pins)},
	{"gpio33_grp", gpio33_pins, ARRAY_SIZE(gpio33_pins)},
	{"gpio34_grp", gpio34_pins, ARRAY_SIZE(gpio34_pins)},
	{"gpio35_grp", gpio35_pins, ARRAY_SIZE(gpio35_pins)},
	{"gpio36_grp", gpio36_pins, ARRAY_SIZE(gpio36_pins)},
	{"gpio37_grp", gpio37_pins, ARRAY_SIZE(gpio37_pins)},
	{"gpio38_grp", gpio38_pins, ARRAY_SIZE(gpio38_pins)},
	{"gpio39_grp", gpio39_pins, ARRAY_SIZE(gpio39_pins)},
	{"gpio40_grp", gpio40_pins, ARRAY_SIZE(gpio40_pins)},
	{"gpio41_grp", gpio41_pins, ARRAY_SIZE(gpio41_pins)},
	{"gpio42_grp", gpio42_pins, ARRAY_SIZE(gpio42_pins)},
	{"gpio43_grp", gpio43_pins, ARRAY_SIZE(gpio43_pins)},
	{"gpio44_grp", gpio44_pins, ARRAY_SIZE(gpio44_pins)},
	{"gpio45_grp", gpio45_pins, ARRAY_SIZE(gpio45_pins)},
	{"gpio46_grp", gpio46_pins, ARRAY_SIZE(gpio46_pins)},
	{"gpio47_grp", gpio47_pins, ARRAY_SIZE(gpio47_pins)},
	{"gpio48_grp", gpio48_pins, ARRAY_SIZE(gpio48_pins)},
	{"gpio49_grp", gpio49_pins, ARRAY_SIZE(gpio49_pins)},
	{"gpio50_grp", gpio50_pins, ARRAY_SIZE(gpio50_pins)},
	{"gpio51_grp", gpio51_pins, ARRAY_SIZE(gpio51_pins)},
	{"gpio52_grp", gpio52_pins, ARRAY_SIZE(gpio52_pins)},
	{"gpio53_grp", gpio53_pins, ARRAY_SIZE(gpio53_pins)},
	{"gpio54_grp", gpio54_pins, ARRAY_SIZE(gpio54_pins)},
	{"gpio55_grp", gpio55_pins, ARRAY_SIZE(gpio55_pins)},
	{"gpio56_grp", gpio56_pins, ARRAY_SIZE(gpio56_pins)},
	{"gpio57_grp", gpio57_pins, ARRAY_SIZE(gpio57_pins)},
	{"gpio58_grp", gpio58_pins, ARRAY_SIZE(gpio58_pins)},
	{"gpio59_grp", gpio59_pins, ARRAY_SIZE(gpio59_pins)},
	{"gpio60_grp", gpio60_pins, ARRAY_SIZE(gpio60_pins)},
	{"gpio61_grp", gpio61_pins, ARRAY_SIZE(gpio61_pins)},
	{"gpio62_grp", gpio62_pins, ARRAY_SIZE(gpio62_pins)},
	{"gpio63_grp", gpio63_pins, ARRAY_SIZE(gpio63_pins)},
	{"gpio64_grp", gpio64_pins, ARRAY_SIZE(gpio64_pins)},
	{"gpio65_grp", gpio65_pins, ARRAY_SIZE(gpio65_pins)},
	{"gpio66_grp", gpio66_pins, ARRAY_SIZE(gpio66_pins)},
	{"gpio67_grp", gpio67_pins, ARRAY_SIZE(gpio67_pins)},
	{"eth1_grp", eth1_pins, ARRAY_SIZE(eth1_pins)},
	{"i2s0_grp", i2s0_pins, ARRAY_SIZE(i2s0_pins)},
	{"i2s0_mclkin_grp", i2s0_mclkin_pins, ARRAY_SIZE(i2s0_mclkin_pins)},
	{"i2s1_grp", i2s1_pins, ARRAY_SIZE(i2s1_pins)},
	{"i2s1_mclkin_grp", i2s1_mclkin_pins, ARRAY_SIZE(i2s1_mclkin_pins)},
	{"spi0_grp", spi0_pins, ARRAY_SIZE(spi0_pins)},
	{"null_grp", null_pins, 0},
};

struct bm_pmx_func {
	const char *name;
	const char * const *groups;
	const unsigned int num_groups;
};

static const char * const nanad_group[] = {"nand_grp"};
static const char * const spi_group[] = {"spi_grp"};
static const char * const emmc_group[] = {"emmc_grp"};
static const char * const sdio_group[] = {"sdio_grp"};
static const char * const eth0_group[] = {"eth0_grp"};
static const char * const pwm0_group[] = {"pwm0_grp"};
static const char * const pwm1_group[] = {"pwm1_grp"};
static const char * const pwm2_group[] = {"pwm2_grp"};
static const char * const pwm3_group[] = {"pwm3_grp"};
static const char * const pwm4_group[] = {"pwm4_grp"};
static const char * const pwm5_group[] = {"pwm5_grp"};
static const char * const pwm6_group[] = {"pwm6_grp"};
static const char * const pwm7_group[] = {"pwm7_grp"};
static const char * const pwm8_group[] = {"pwm8_grp"};
static const char * const pwm9_group[] = {"pwm9_grp"};
static const char * const pwm10_group[] = {"pwm10_grp"};
static const char * const pwm11_group[] = {"pwm11_grp"};
static const char * const pwm12_group[] = {"pwm12_grp"};
static const char * const pwm13_group[] = {"pwm13_grp"};
static const char * const pwm14_group[] = {"pwm14_grp"};
static const char * const pwm15_group[] = {"pwm15_grp"};
static const char * const pwm16_group[] = {"pwm16_grp"};
static const char * const pwm17_group[] = {"pwm17_grp"};
static const char * const pwm18_group[] = {"pwm18_grp"};
static const char * const pwm19_group[] = {"pwm19_grp"};
static const char * const pwm20_group[] = {"pwm20_grp"};
static const char * const pwm21_group[] = {"pwm21_grp"};
static const char * const pwm22_group[] = {"pwm22_grp"};
static const char * const pwm23_group[] = {"pwm23_grp"};
static const char * const pwm24_group[] = {"pwm24_grp"};
static const char * const pwm25_group[] = {"pwm25_grp"};
static const char * const pwm26_group[] = {"pwm26_grp"};
static const char * const pwm27_group[] = {"pwm27_grp"};
static const char * const pwm28_group[] = {"pwm28_grp"};
static const char * const pwm29_group[] = {"pwm29_grp"};
static const char * const pwm30_group[] = {"pwm30_grp"};
static const char * const pwm31_group[] = {"pwm31_grp"};
static const char * const pwm32_group[] = {"pwm32_grp"};
static const char * const pwm33_group[] = {"pwm33_grp"};
static const char * const pwm34_group[] = {"pwm34_grp"};
static const char * const pwm35_group[] = {"pwm35_grp"};
static const char * const pwm36_group[] = {"pwm36_grp"};
static const char * const pwm37_group[] = {"pwm37_grp"};
static const char * const i2c0_group[] = {"i2c0_grp"};
static const char * const i2c1_group[] = {"i2c1_grp"};
static const char * const i2c2_group[] = {"i2c2_grp"};
static const char * const i2c3_group[] = {"i2c3_grp"};
static const char * const i2c4_group[] = {"i2c4_grp"};
static const char * const uart0_group[] = {"uart0_grp"};
static const char * const uart1_group[] = {"uart1_grp"};
static const char * const uart2_group[] = {"uart2_grp"};
static const char * const uart3_group[] = {"uart3_grp"};
static const char * const uart4_group[] = {"uart4_grp"};
static const char * const uart5_group[] = {"uart5_grp"};
static const char * const uart6_group[] = {"uart6_grp"};
static const char * const uart7_group[] = {"uart7_grp"};
static const char * const uart8_group[] = {"uart8_grp"};
static const char * const uart9_group[] = {"uart9_grp"};
static const char * const uart10_group[] = {"uart10_grp"};
static const char * const uart11_group[] = {"uart11_grp"};
static const char * const gpio0_group[] = {"gpio0_grp"};
static const char * const gpio1_group[] = {"gpio1_grp"};
static const char * const gpio2_group[] = {"gpio2_grp"};
static const char * const gpio3_group[] = {"gpio3_grp"};
static const char * const gpio4_group[] = {"gpio4_grp"};
static const char * const gpio5_group[] = {"gpio5_grp"};
static const char * const gpio6_group[] = {"gpio6_grp"};
static const char * const gpio7_group[] = {"gpio7_grp"};
static const char * const gpio8_group[] = {"gpio8_grp"};
static const char * const gpio9_group[] = {"gpio9_grp"};
static const char * const gpio10_group[] = {"gpio10_grp"};
static const char * const gpio11_group[] = {"gpio11_grp"};
static const char * const gpio12_group[] = {"gpio12_grp"};
static const char * const gpio13_group[] = {"gpio13_grp"};
static const char * const gpio14_group[] = {"gpio14_grp"};
static const char * const gpio15_group[] = {"gpio15_grp"};
static const char * const gpio16_group[] = {"gpio16_grp"};
static const char * const gpio17_group[] = {"gpio17_grp"};
static const char * const gpio18_group[] = {"gpio18_grp"};
static const char * const gpio19_group[] = {"gpio19_grp"};
static const char * const gpio20_group[] = {"gpio20_grp"};
static const char * const gpio21_group[] = {"gpio21_grp"};
static const char * const gpio22_group[] = {"gpio22_grp"};
static const char * const gpio23_group[] = {"gpio23_grp"};
static const char * const gpio24_group[] = {"gpio24_grp"};
static const char * const gpio25_group[] = {"gpio25_grp"};
static const char * const gpio26_group[] = {"gpio26_grp"};
static const char * const gpio27_group[] = {"gpio27_grp"};
static const char * const gpio28_group[] = {"gpio28_grp"};
static const char * const gpio29_group[] = {"gpio29_grp"};
static const char * const gpio30_group[] = {"gpio30_grp"};
static const char * const gpio31_group[] = {"gpio31_grp"};
static const char * const gpio32_group[] = {"gpio32_grp"};
static const char * const gpio33_group[] = {"gpio33_grp"};
static const char * const gpio34_group[] = {"gpio34_grp"};
static const char * const gpio35_group[] = {"gpio35_grp"};
static const char * const gpio36_group[] = {"gpio36_grp"};
static const char * const gpio37_group[] = {"gpio37_grp"};
static const char * const gpio38_group[] = {"gpio38_grp"};
static const char * const gpio39_group[] = {"gpio39_grp"};
static const char * const gpio40_group[] = {"gpio40_grp"};
static const char * const gpio41_group[] = {"gpio41_grp"};
static const char * const gpio42_group[] = {"gpio42_grp"};
static const char * const gpio43_group[] = {"gpio43_grp"};
static const char * const gpio44_group[] = {"gpio44_grp"};
static const char * const gpio45_group[] = {"gpio45_grp"};
static const char * const gpio46_group[] = {"gpio46_grp"};
static const char * const gpio47_group[] = {"gpio47_grp"};
static const char * const gpio48_group[] = {"gpio48_grp"};
static const char * const gpio49_group[] = {"gpio49_grp"};
static const char * const gpio50_group[] = {"gpio50_grp"};
static const char * const gpio51_group[] = {"gpio51_grp"};
static const char * const gpio52_group[] = {"gpio52_grp"};
static const char * const gpio53_group[] = {"gpio53_grp"};
static const char * const gpio54_group[] = {"gpio54_grp"};
static const char * const gpio55_group[] = {"gpio55_grp"};
static const char * const gpio56_group[] = {"gpio56_grp"};
static const char * const gpio57_group[] = {"gpio57_grp"};
static const char * const gpio58_group[] = {"gpio58_grp"};
static const char * const gpio59_group[] = {"gpio59_grp"};
static const char * const gpio60_group[] = {"gpio60_grp"};
static const char * const gpio61_group[] = {"gpio61_grp"};
static const char * const gpio62_group[] = {"gpio62_grp"};
static const char * const gpio63_group[] = {"gpio63_grp"};
static const char * const gpio64_group[] = {"gpio64_grp"};
static const char * const gpio65_group[] = {"gpio65_grp"};
static const char * const gpio66_group[] = {"gpio66_grp"};
static const char * const gpio67_group[] = {"gpio67_grp"};
static const char * const eth1_group[] = {"eth1_grp"};
static const char * const i2s0_group[] = {"i2s0_grp"};
static const char * const i2s0_mclkin_group[] = {"i2s0_mclkin_grp"};
static const char * const i2s1_group[] = {"i2s1_grp"};
static const char * const i2s1_mclkin_group[] = {"i2s1_mclkin_grp"};
static const char * const spi0_group[] = {"spi0_grp"};
static const char * const null_group[] = {"null_grp"};


enum {F_NAND, F_SPI, F_EMMC, F_SDIO, F_ETH0,
				F_PWM0, F_PWM1, F_PWM2, F_PWM3, F_PWM4, F_PWM5,
				F_PWM6, F_PWM7, F_PWM8, F_PWM9, F_PWM10, F_PWM11,
				F_PWM12, F_PWM13, F_PWM14, F_PWM15, F_PWM16, F_PWM17,
				F_PWM18, F_PWM19, F_PWM20, F_PWM21, F_PWM22, F_PWM23,
				F_PWM24, F_PWM25, F_PWM26, F_PWM27, F_PWM28, F_PWM29,
				F_PWM30, F_PWM31, F_PWM32, F_PWM33, F_PWM34, F_PWM35,
				F_PWM36, F_PWM37,
				F_I2C0, F_I2C1, F_I2C2, F_I2C3, F_I2C4,
				F_UART0, F_UART1, F_UART2, F_UART3, F_UART4,
				F_UART5, F_UART6, F_UART7, F_UART8, F_UART9,
				F_UART10, F_UART11,
				F_GPIO0, F_GPIO1, F_GPIO2, F_GPIO3, F_GPIO4,
				F_GPIO5, F_GPIO6, F_GPIO7, F_GPIO8, F_GPIO9,
				F_GPIO10, F_GPIO11, F_GPIO12, F_GPIO13, F_GPIO14,
				F_GPIO15, F_GPIO16, F_GPIO17, F_GPIO18, F_GPIO19,
				F_GPIO20, F_GPIO21, F_GPIO22, F_GPIO23, F_GPIO24,
				F_GPIO25, F_GPIO26, F_GPIO27, F_GPIO28, F_GPIO29,
				F_GPIO30, F_GPIO31, F_GPIO32, F_GPIO33, F_GPIO34,
				F_GPIO35, F_GPIO36, F_GPIO37, F_GPIO38, F_GPIO39,
				F_GPIO40, F_GPIO41, F_GPIO42, F_GPIO43, F_GPIO44,
				F_GPIO45, F_GPIO46, F_GPIO47, F_GPIO48, F_GPIO49,
				F_GPIO50, F_GPIO51, F_GPIO52, F_GPIO53, F_GPIO54,
				F_GPIO55, F_GPIO56, F_GPIO57, F_GPIO58, F_GPIO59,
				F_GPIO60, F_GPIO61, F_GPIO62, F_GPIO63, F_GPIO64,
				F_GPIO65, F_GPIO66, F_GPIO67,
				F_ETH1,
				F_I2S0, F_I2S0_MCLKIN, F_I2S1, F_I2S1_MCLKIN,
				F_SPI0,
				F_ENDMARK} func_idx;

unsigned int pmux_val[F_ENDMARK] = {
	[F_NAND] = 2, [F_SPI] = 0, [F_EMMC] = 1, [F_SDIO] = 0, [F_ETH0] = 0,
	[F_PWM0 ... F_PWM37] = 2,
	[F_I2C0 ... F_I2C4] = 1,
	[F_UART0 ... F_UART11] = 1,
	[F_GPIO0 ... F_GPIO9] = 0, [F_GPIO13] = 1, [F_ETH1] = 1,
	[F_I2S0] = 2, [F_I2S0_MCLKIN] = 1, [F_I2S1] = 2, [F_I2S1_MCLKIN] = 1,
	[F_SPI0] = 1,};


static const struct bm_pmx_func bm_funcs[] = {
	{"nand_a", nanad_group, 1},
	{"spi_a", spi_group, 1},
	{"emmc_a", emmc_group, 1},
	{"sdio_a", sdio_group, 1},
	{"eth0_a", eth0_group, 1},
	{"pwm0_a", pwm0_group, 1},
	{"pwm1_a", pwm1_group, 1},
	{"pwm2_a", pwm2_group, 1},
	{"pwm3_a", pwm3_group, 1},
	{"pwm4_a", pwm4_group, 1},
	{"pwm5_a", pwm5_group, 1},
	{"pwm6_a", pwm6_group, 1},
	{"pwm7_a", pwm7_group, 1},
	{"pwm8_a", pwm8_group, 1},
	{"pwm9_a", pwm9_group, 1},
	{"pwm10_a", pwm10_group, 1},
	{"pwm11_a", pwm11_group, 1},
	{"pwm12_a", pwm12_group, 1},
	{"pwm13_a", pwm13_group, 1},
	{"pwm14_a", pwm14_group, 1},
	{"pwm15_a", pwm15_group, 1},
	{"pwm16_a", pwm16_group, 1},
	{"pwm17_a", pwm17_group, 1},
	{"pwm18_a", pwm18_group, 1},
	{"pwm19_a", pwm19_group, 1},
	{"pwm20_a", pwm20_group, 1},
	{"pwm21_a", pwm21_group, 1},
	{"pwm22_a", pwm22_group, 1},
	{"pwm23_a", pwm23_group, 1},
	{"pwm24_a", pwm24_group, 1},
	{"pwm25_a", pwm25_group, 1},
	{"pwm26_a", pwm26_group, 1},
	{"pwm27_a", pwm27_group, 1},
	{"pwm28_a", pwm28_group, 1},
	{"pwm29_a", pwm29_group, 1},
	{"pwm30_a", pwm30_group, 1},
	{"pwm31_a", pwm31_group, 1},
	{"pwm32_a", pwm32_group, 1},
	{"pwm33_a", pwm33_group, 1},
	{"pwm34_a", pwm34_group, 1},
	{"pwm35_a", pwm35_group, 1},
	{"pwm36_a", pwm36_group, 1},
	{"pwm37_a", pwm37_group, 1},
	{"i2c0_a", i2c0_group, 1},
	{"i2c1_a", i2c1_group, 1},
	{"i2c2_a", i2c2_group, 1},
	{"i2c3_a", i2c3_group, 1},
	{"i2c4_a", i2c4_group, 1},
	{"uart0_a", uart0_group, 1},
	{"uart1_a", uart1_group, 1},
	{"uart2_a", uart2_group, 1},
	{"uart3_a", uart3_group, 1},
	{"uart4_a", uart4_group, 1},
	{"uart5_a", uart5_group, 1},
	{"uart6_a", uart6_group, 1},
	{"uart7_a", uart7_group, 1},
	{"uart8_a", uart8_group, 1},
	{"uart9_a", uart9_group, 1},
	{"uart10_a", uart10_group, 1},
	{"uart11_a", uart11_group, 1},
	{"gpio0_a", gpio0_group, 1},
	{"gpio1_a", gpio1_group, 1},
	{"gpio2_a", gpio2_group, 1},
	{"gpio3_a", gpio3_group, 1},
	{"gpio4_a", gpio4_group, 1},
	{"gpio5_a", gpio5_group, 1},
	{"gpio6_a", gpio6_group, 1},
	{"gpio7_a", gpio7_group, 1},
	{"gpio8_a", gpio8_group, 1},
	{"gpio9_a", gpio9_group, 1},
	{"gpio10_a", gpio10_group, 1},
	{"gpio11_a", gpio11_group, 1},
	{"gpio12_a", gpio12_group, 1},
	{"gpio13_a", gpio13_group, 1},
	{"gpio14_a", gpio14_group, 1},
	{"gpio15_a", gpio15_group, 1},
	{"gpio16_a", gpio16_group, 1},
	{"gpio17_a", gpio17_group, 1},
	{"gpio18_a", gpio18_group, 1},
	{"gpio19_a", gpio19_group, 1},
	{"gpio20_a", gpio20_group, 1},
	{"gpio21_a", gpio21_group, 1},
	{"gpio22_a", gpio22_group, 1},
	{"gpio23_a", gpio23_group, 1},
	{"gpio24_a", gpio24_group, 1},
	{"gpio25_a", gpio25_group, 1},
	{"gpio26_a", gpio26_group, 1},
	{"gpio27_a", gpio27_group, 1},
	{"gpio28_a", gpio28_group, 1},
	{"gpio29_a", gpio29_group, 1},
	{"gpio30_a", gpio30_group, 1},
	{"gpio31_a", gpio31_group, 1},
	{"gpio32_a", gpio32_group, 1},
	{"gpio33_a", gpio33_group, 1},
	{"gpio34_a", gpio34_group, 1},
	{"gpio35_a", gpio35_group, 1},
	{"gpio36_a", gpio36_group, 1},
	{"gpio37_a", gpio37_group, 1},
	{"gpio38_a", gpio38_group, 1},
	{"gpio39_a", gpio39_group, 1},
	{"gpio40_a", gpio40_group, 1},
	{"gpio41_a", gpio41_group, 1},
	{"gpio42_a", gpio42_group, 1},
	{"gpio43_a", gpio43_group, 1},
	{"gpio44_a", gpio44_group, 1},
	{"gpio45_a", gpio45_group, 1},
	{"gpio46_a", gpio46_group, 1},
	{"gpio47_a", gpio47_group, 1},
	{"gpio48_a", gpio48_group, 1},
	{"gpio49_a", gpio49_group, 1},
	{"gpio50_a", gpio50_group, 1},
	{"gpio51_a", gpio51_group, 1},
	{"gpio52_a", gpio52_group, 1},
	{"gpio53_a", gpio53_group, 1},
	{"gpio54_a", gpio54_group, 1},
	{"gpio55_a", gpio55_group, 1},
	{"gpio56_a", gpio56_group, 1},
	{"gpio57_a", gpio57_group, 1},
	{"gpio58_a", gpio58_group, 1},
	{"gpio59_a", gpio59_group, 1},
	{"gpio60_a", gpio60_group, 1},
	{"gpio61_a", gpio61_group, 1},
	{"gpio62_a", gpio62_group, 1},
	{"gpio63_a", gpio63_group, 1},
	{"gpio64_a", gpio64_group, 1},
	{"gpio65_a", gpio65_group, 1},
	{"gpio66_a", gpio66_group, 1},
	{"gpio67_a", gpio67_group, 1},
	{"eth1_a", eth1_group, 1},
	{"i2s0_a", i2s0_group, 1},
	{"i2s0_mclkin_a", i2s0_mclkin_group, 1},
	{"i2s1_a", i2s1_group, 1},
	{"i2s1_mclkin_a", i2s1_mclkin_group, 1},
	{"spi0_a", spi0_group, 1},
	{"nand_r", null_group, 1},
	{"spi_r", null_group, 1},
	{"emmc_r", null_group, 1},
	{"sdio_r", null_group, 1},
	{"eth0_r", null_group, 1},
	{"pwm0_r", null_group, 1},
	{"pwm1_r", null_group, 1},
	{"pwm2_r", null_group, 1},
	{"pwm3_r", null_group, 1},
	{"pwm4_r", null_group, 1},
	{"pwm5_r", null_group, 1},
	{"pwm6_r", null_group, 1},
	{"pwm7_r", null_group, 1},
	{"pwm8_r", null_group, 1},
	{"pwm9_r", null_group, 1},
	{"pwm10_r", null_group, 1},
	{"pwm11_r", null_group, 1},
	{"pwm12_r", null_group, 1},
	{"pwm13_r", null_group, 1},
	{"pwm14_r", null_group, 1},
	{"pwm15_r", null_group, 1},
	{"pwm16_r", null_group, 1},
	{"pwm17_r", null_group, 1},
	{"pwm18_r", null_group, 1},
	{"pwm19_r", null_group, 1},
	{"pwm20_r", null_group, 1},
	{"pwm21_r", null_group, 1},
	{"pwm22_r", null_group, 1},
	{"pwm23_r", null_group, 1},
	{"pwm24_r", null_group, 1},
	{"pwm25_r", null_group, 1},
	{"pwm26_r", null_group, 1},
	{"pwm27_r", null_group, 1},
	{"pwm28_r", null_group, 1},
	{"pwm29_r", null_group, 1},
	{"pwm30_r", null_group, 1},
	{"pwm31_r", null_group, 1},
	{"pwm32_r", null_group, 1},
	{"pwm33_r", null_group, 1},
	{"pwm34_r", null_group, 1},
	{"pwm35_r", null_group, 1},
	{"pwm36_r", null_group, 1},
	{"pwm37_r", null_group, 1},
	{"i2c0_r", null_group, 1},
	{"i2c1_r", null_group, 1},
	{"i2c2_r", null_group, 1},
	{"i2c3_r", null_group, 1},
	{"i2c4_r", null_group, 1},
	{"uart0_r", null_group, 1},
	{"uart1_r", null_group, 1},
	{"uart2_r", null_group, 1},
	{"uart3_r", null_group, 1},
	{"uart4_r", null_group, 1},
	{"uart5_r", null_group, 1},
	{"uart6_r", null_group, 1},
	{"uart7_r", null_group, 1},
	{"uart8_r", null_group, 1},
	{"uart9_r", null_group, 1},
	{"uart10_r", null_group, 1},
	{"uart11_r", null_group, 1},
	{"gpio0_r", null_group, 1},
	{"gpio1_r", null_group, 1},
	{"gpio2_r", null_group, 1},
	{"gpio3_r", null_group, 1},
	{"gpio4_r", null_group, 1},
	{"gpio5_r", null_group, 1},
	{"gpio6_r", null_group, 1},
	{"gpio7_r", null_group, 1},
	{"gpio8_r", null_group, 1},
	{"gpio9_r", null_group, 1},
	{"gpio10_r", null_group, 1},
	{"gpio11_r", null_group, 1},
	{"gpio12_r", null_group, 1},
	{"gpio13_r", null_group, 1},
	{"gpio14_r", null_group, 1},
	{"gpio15_r", null_group, 1},
	{"gpio16_r", null_group, 1},
	{"gpio17_r", null_group, 1},
	{"gpio18_r", null_group, 1},
	{"gpio19_r", null_group, 1},
	{"gpio20_r", null_group, 1},
	{"gpio21_r", null_group, 1},
	{"gpio22_r", null_group, 1},
	{"gpio23_r", null_group, 1},
	{"gpio24_r", null_group, 1},
	{"gpio25_r", null_group, 1},
	{"gpio26_r", null_group, 1},
	{"gpio27_r", null_group, 1},
	{"gpio28_r", null_group, 1},
	{"gpio29_r", null_group, 1},
	{"gpio30_r", null_group, 1},
	{"gpio31_r", null_group, 1},
	{"gpio32_r", null_group, 1},
	{"gpio33_r", null_group, 1},
	{"gpio34_r", null_group, 1},
	{"gpio35_r", null_group, 1},
	{"gpio36_r", null_group, 1},
	{"gpio37_r", null_group, 1},
	{"gpio38_r", null_group, 1},
	{"gpio39_r", null_group, 1},
	{"gpio40_r", null_group, 1},
	{"gpio41_r", null_group, 1},
	{"gpio42_r", null_group, 1},
	{"gpio43_r", null_group, 1},
	{"gpio44_r", null_group, 1},
	{"gpio45_r", null_group, 1},
	{"gpio46_r", null_group, 1},
	{"gpio47_r", null_group, 1},
	{"gpio48_r", null_group, 1},
	{"gpio49_r", null_group, 1},
	{"gpio50_r", null_group, 1},
	{"gpio51_r", null_group, 1},
	{"gpio52_r", null_group, 1},
	{"gpio53_r", null_group, 1},
	{"gpio54_r", null_group, 1},
	{"gpio55_r", null_group, 1},
	{"gpio56_r", null_group, 1},
	{"gpio57_r", null_group, 1},
	{"gpio58_r", null_group, 1},
	{"gpio59_r", null_group, 1},
	{"gpio60_r", null_group, 1},
	{"gpio61_r", null_group, 1},
	{"gpio62_r", null_group, 1},
	{"gpio63_r", null_group, 1},
	{"gpio64_r", null_group, 1},
	{"gpio65_r", null_group, 1},
	{"gpio66_r", null_group, 1},
	{"gpio67_r", null_group, 1},
	{"eth1_r", null_group, 1},
	{"i2s0_r", null_group, 1},
	{"i2s0_mclkin_r", null_group, 1},
	{"i2s1_r", null_group, 1},
	{"i2s1_mclkin_r", null_group, 1},
	{"spi0_r", null_group, 1},
};

static struct pinctrl_state *nand_a, *nand_r, *spi_a, *spi_r, *emmc_a, *emmc_r,
							*uart1_a, *uart1_r, *uart2_a, *uart2_r, *uart3_a, *uart3_r,
							*uart4_a, *uart4_r, *uart5_a, *uart5_r, *uart6_a, *uart6_r,
							*uart7_a, *uart7_r, *uart8_a, *uart8_r, *uart9_a, *uart9_r,
							*uart10_a, *uart10_r, *uart11_a, *uart11_r;

static int bm_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(bm_funcs);
}

static int bm_get_groups(struct pinctrl_dev *pctldev, unsigned int selector,
			  const char * const **groups,
			  unsigned int * const num_groups)
{
	*groups = bm_funcs[selector].groups;
	*num_groups = bm_funcs[selector].num_groups;
	return 0;
}

static const char *bm_get_fname(struct pinctrl_dev *pctldev, unsigned int selector)
{
	return bm_funcs[selector].name;
}



static int bm_set_mux(struct pinctrl_dev *pctldev, unsigned int selector,
		unsigned int group)
{
 // int g;
	int p;
	struct bm_pinctrl *ctrl = pinctrl_dev_get_drvdata(pctldev);

	dev_info(ctrl->dev, "%s seletor:%u group:%u #group:%d $pin:%d\n", __func__, selector,
	group, bm_funcs[selector].num_groups, bm_groups[group].num_pins);
 //   for (g = 0; g < bm_funcs[selector].num_groups; g++) {
		for (p = 0; p < bm_groups[group].num_pins; p++) {
			unsigned int pidx = bm_groups[group].pins[p];
			u32 offset = ((pidx>>1)<<2);
			u32 regval = readl(ctrl->virtbase + offset);
			u32 mux_offset = ((!((pidx+1) & 1) << 4) + 4);
			dev_info(ctrl->dev, "ctrl->virtbase:%p pin:%d oldreg val=0x%X offset:0x%x mux_offset:%u pmux_val:%x\n",
			ctrl->virtbase, pidx, regval, offset, mux_offset, pmux_val[selector]);
			regval = regval & ~(3 << mux_offset);
			regval |= pmux_val[selector] << mux_offset;
			//printk("new reg val=0x%X\n", regval);
			writel(regval, ctrl->virtbase + offset);
			regval = readl(ctrl->virtbase + offset);
			dev_info(ctrl->dev, "check new reg val=0x%X\n", regval);
		}
  //  }
	return 0;//0:ok 1:error
}


static const struct pinmux_ops bm_pinmux_ops = {
	.get_functions_count = bm_get_functions_count,
	.get_function_name = bm_get_fname,
	.get_function_groups = bm_get_groups,
	.set_mux = bm_set_mux,
	.strict = true,
};



static int bm_pinconf_cfg_get(struct pinctrl_dev *pctldev, unsigned int pin,
				   unsigned long *config)
{
	return 0;
}


static int bm_pinconf_cfg_set(struct pinctrl_dev *pctldev, unsigned int pin,
				   unsigned long *configs, unsigned int num_configs)
{
	return 0;
}


static int bm_pinconf_group_set(struct pinctrl_dev *pctldev,
				  unsigned int selector,
				  unsigned long *configs,
				  unsigned int num_configs)
{
	return 0;
}

static const struct pinconf_ops bm_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = bm_pinconf_cfg_get,
	.pin_config_set = bm_pinconf_cfg_set,
	.pin_config_group_set = bm_pinconf_group_set,
};


static int bm_get_groups_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(bm_groups);
}


static const char *bm_get_group_name(struct pinctrl_dev *pctldev,
					   unsigned int selector)
{
	return bm_groups[selector].name;
}


static int bm_get_group_pins(struct pinctrl_dev *pctldev, unsigned int selector,
				   const unsigned int **pins,
				   unsigned int *num_pins)
{
	*pins = bm_groups[selector].pins;
	*num_pins = bm_groups[selector].num_pins;
	return 0;
}


static void bm_pin_dbg_show(struct pinctrl_dev *pctldev, struct seq_file *s,
		   unsigned int offset)
{
}


static const struct pinctrl_ops bm_pctrl_ops = {
	.get_groups_count = bm_get_groups_count,
	.get_group_name = bm_get_group_name,
	.get_group_pins = bm_get_group_pins,
	.pin_dbg_show = bm_pin_dbg_show,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
};


static struct pinctrl_desc bm_desc = {
	.name = "bm_pinctrl",
	.pins = bm_pins,
	.npins = ARRAY_SIZE(bm_pins),
	.pctlops = &bm_pctrl_ops,
	.pmxops = &bm_pinmux_ops,
	.confops = &bm_pinconf_ops,
	//.num_custom_params = ARRAY_SIZE(zynq_dt_params),
	//.custom_params = zynq_dt_params,
#ifdef CONFIG_DEBUG_FS
	//.custom_conf_items = bm_conf_items,
#endif
	.owner = THIS_MODULE,
};

static ssize_t mux_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char test[] = " mux_sysfs_show:OK";

	dev_info(dev, "%s\n", test);
	return sprintf(buf, "%s\n", test);
}

static ssize_t mux_sysfs_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long user_data;
	unsigned long state;
	const struct bm_dev_mux *dev_mux = dev_get_drvdata(dev);
	const char *drv_name = dev_driver_string(dev);
	struct pinctrl *p;
	int ret;

	state = kstrtoul(buf, 0, &user_data);
	p = pinctrl_get(dev);
	if (IS_ERR(p))
		return PTR_ERR(p);
	dev_info(dev, "got data from user space: %lu\n", user_data);

	if (!strcmp(drv_name, "bm-nand-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, nand_a);
		} else{
			ret = pinctrl_select_state(p, nand_r);
		}
	} else if (!strcmp(drv_name, "bm-spi-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, spi_a);
		} else{
			ret = pinctrl_select_state(p, spi_r);
		}
	} else if (!strcmp(drv_name, "bm-emmc-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, emmc_a);
		} else{
			ret = pinctrl_select_state(p, emmc_r);
		}
	} else if (!strcmp(drv_name, "bm-uart1-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart1_a);
		} else{
			ret = pinctrl_select_state(p, uart1_r);
		}
	} else if (!strcmp(drv_name, "bm-uart2-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart2_a);
		} else{
			ret = pinctrl_select_state(p, uart2_r);
		}
	} else if (!strcmp(drv_name, "bm-uart3-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart3_a);
		} else{
			ret = pinctrl_select_state(p, uart3_r);
		}
	} else if (!strcmp(drv_name, "bm-uart4-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart4_a);
		} else{
			ret = pinctrl_select_state(p, uart4_r);
		}
	} else if (!strcmp(drv_name, "bm-uart5-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart5_a);
		} else{
			ret = pinctrl_select_state(p, uart5_r);
		}
	} else if (!strcmp(drv_name, "bm-uart6-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart6_a);
		} else{
			ret = pinctrl_select_state(p, uart6_r);
		}
	} else if (!strcmp(drv_name, "bm-uart7-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart7_a);
		} else{
			ret = pinctrl_select_state(p, uart7_r);
		}
	} else if (!strcmp(drv_name, "bm-uart8-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart8_a);
		} else{
			ret = pinctrl_select_state(p, uart8_r);
		}
	} else if (!strcmp(drv_name, "bm-uart9-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart9_a);
		} else{
			ret = pinctrl_select_state(p, uart9_r);
		}
	} else if (!strcmp(drv_name, "bm-uart10-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart10_a);
		} else{
			ret = pinctrl_select_state(p, uart10_r);
		}
	} else if (!strcmp(drv_name, "bm-uart11-mux")) {
		if (user_data) {
			ret = pinctrl_select_state(p, uart11_a);
		} else{
			ret = pinctrl_select_state(p, uart11_r);
		}
	} else if (!strcmp(drv_name, "bm-pwm-mux")) {
		if (user_data) {
			void __iomem *base;
			u32 regval;
			//set the 1st level MUX(from PWM module)
			const u32 *first_mux_base = of_get_property(dev_mux->dev->of_node, "source_mux", NULL);
			const u32 *first_mux_offset = of_get_property(dev_mux->dev->of_node, "mux_offset", NULL);
			u32 offset = be32_to_cpup(first_mux_offset);

			if (!first_mux_base || !first_mux_offset)
				return -ENOENT;
			dev_info(dev, "first_mux_base:%x offset:%x user_data:%lx\n",
					*first_mux_base, *first_mux_offset, user_data);
			base = ioremap(be32_to_cpup(first_mux_base), 4096);
			dev_info(dev, "first_mux_base:%x(va:%p) offset:0x%x user_data-1:0x%lx\n",
					be32_to_cpup(first_mux_base), base, offset, user_data-1);
			if (!base) {
				pr_err("failed to remap base\n");
				return -ENOMEM;
			}
			regval = readl(base);
			regval = regval & ~(0xF << offset);
			regval |= (user_data-1) << offset;
			writel(regval, base);
			regval = readl(base);
			dev_info(dev, "check result:%x\n", regval);
			iounmap(base);
			ret = pinctrl_select_state(p, dev_mux->acquire);
		} else {
			ret = pinctrl_select_state(p, dev_mux->release);
		}
	} else if (!strcmp(drv_name, "bm-gpio-mux")) {
		if (user_data) {
			void __iomem *base;
			u32 regval;
			const u32 *first_mux_base = of_get_property(dev_mux->dev->of_node, "source_mux", NULL);
			const u32 *first_mux_offset = of_get_property(dev_mux->dev->of_node, "mux_offset", NULL);
			const u32 *first_mux_value = of_get_property(dev_mux->dev->of_node, "mux_value", NULL);
			u32 offset = be32_to_cpup(first_mux_offset);
			u32 value = be32_to_cpup(first_mux_value);

			if (!first_mux_base || !first_mux_offset)
				return -ENOENT;

			dev_info(dev, "first_mux_base:%x offset:%x user_data:%lx\n",
					*first_mux_base, *first_mux_offset, user_data);
			base = ioremap(be32_to_cpup(first_mux_base), 4096);
			dev_info(dev, "first_mux_base:%x(va:%p) offset:0x%x\n",
					be32_to_cpup(first_mux_base), base, offset);

			if (!base) {
				pr_err("failed to remap base\n");
				return -ENOMEM;
			}
			regval = readl(base);
			regval = regval & ~(0x3 << offset); // clear pinmux reg_pin_mux_sel
			regval |= value << offset;          // set pinmux reg_pin_mux_sel==2'b00/01/10/11
			writel(regval, base);
			regval = readl(base);
			dev_info(dev, "check result:%x\n", regval);
			iounmap(base);
			ret = pinctrl_select_state(p, dev_mux->acquire);
		} else {
			ret = pinctrl_select_state(p, dev_mux->release);
		}
	} else {
		//The new framework...experimental
		if (user_data)
			ret = pinctrl_select_state(p, dev_mux->acquire);
		else
			ret = pinctrl_select_state(p, dev_mux->release);
	}

	if (ret < 0)
		return (ssize_t)ERR_PTR(ret);
	else
		return size;
}



static DEVICE_ATTR(bm_mux, 0660, mux_sysfs_show, mux_sysfs_store);


static int bm_pinctrl_probe(struct platform_device *pdev)
{
	struct bm_pinctrl *bmpctrl;
	int ret;
	struct resource *res;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	bmpctrl = devm_kzalloc(&pdev->dev, sizeof(*bmpctrl), GFP_KERNEL);
	if (!bmpctrl)
		return -ENOMEM;
	bmpctrl->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	bmpctrl->virtbase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(bmpctrl->virtbase))
		return PTR_ERR(bmpctrl->virtbase);
	// dev_info(&pdev->dev, "Get module base:%p\n", bmpctrl->virtbase);

	bmpctrl->virtbase += 0x20; //MUX base
	// dev_info(&pdev->dev, "Get MUX base:%p\n", bmpctrl->virtbase);

	bmpctrl->pctl = devm_pinctrl_register(&pdev->dev, &bm_desc, bmpctrl);
	if (IS_ERR(bmpctrl->pctl)) {
		dev_err(&pdev->dev, "could not register Bitman pin ctrl driver\n");
		return PTR_ERR(bmpctrl->pctl);
	}

	platform_set_drvdata(pdev, bmpctrl);

	bm_class = class_create(THIS_MODULE, "bm-pinctrl");
	ret = device_create_file(&pdev->dev, &dev_attr_bm_mux);
	if (ret)
		return -ENOENT;
	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);

	// dev_info(&pdev->dev, "get pinstatus\n");
	dev_info(&pdev->dev, "initialized Bitman pin control driver\n");
	return 0;
}

static const struct of_device_id bm_pinctrl_of_match[] = {
	{ .compatible = "bitmain,pinctrl-bm1880" },
};


static struct platform_driver bm_pinctrl_driver = {
	.driver = {
		.name = "bm-pinctrl",
		.of_match_table = bm_pinctrl_of_match,
	},
	.probe = bm_pinctrl_probe,
};


static int __init bm_pinctrl_init(void)
{
	return platform_driver_register(&bm_pinctrl_driver);
}
arch_initcall(bm_pinctrl_init);


static void __exit bm_pinctrl_exit(void)
{
	platform_driver_unregister(&bm_pinctrl_driver);
}
module_exit(bm_pinctrl_exit);

//===NAND MUX pesudo-device===



static int bm_nand_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "nand\n");
	nand_a = pinctrl_lookup_state(pctrl, "nand_a");
	if (IS_ERR(nand_a)) {
		dev_err(&pdev->dev, "could not get pin status, nand_a\n");
		return PTR_ERR(nand_a);
	}
	nand_r = pinctrl_lookup_state(pctrl, "nand_r");
	if (IS_ERR(nand_r)) {
		dev_err(&pdev->dev, "could not get pin status, nand_r\n");
		return PTR_ERR(nand_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_nand_mux_of_match[] = {
	{ .compatible = "bitmain,nand-mux" },
	{ }
};
static struct platform_driver bm_nand_mux_driver = {
	.driver = {
		.name = "bm-nand-mux",
		.of_match_table = bm_nand_mux_of_match,
	},
	.probe = bm_nand_mux_probe,
};


static int __init bm_nand_mux_init(void)
{
	return platform_driver_register(&bm_nand_mux_driver);
}
arch_initcall(bm_nand_mux_init);


 //===SPI MUX pesudo-device===

static int bm_spi_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "spi\n");
	spi_a = pinctrl_lookup_state(pctrl, "spi_a");
	if (IS_ERR(spi_a)) {
		dev_err(&pdev->dev, "could not get pin status, spi_a\n");
		return PTR_ERR(spi_a);
	}
	spi_r = pinctrl_lookup_state(pctrl, "spi_r");
	if (IS_ERR(spi_r)) {
		dev_err(&pdev->dev, "could not get pin status, spi_r\n");
		return PTR_ERR(spi_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_spi_mux_of_match[] = {
	{ .compatible = "bitmain,spi-mux" },
	{ }
};
static struct platform_driver bm_spi_mux_driver = {
	.driver = {
		.name = "bm-spi-mux",
		.of_match_table = bm_spi_mux_of_match,
	},
	.probe = bm_spi_mux_probe,
};


static int __init bm_spi_mux_init(void)
{
	return platform_driver_register(&bm_spi_mux_driver);
}
arch_initcall(bm_spi_mux_init);

//==EMMC==

static int bm_emmc_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "emmc\n");
	emmc_a = pinctrl_lookup_state(pctrl, "emmc_a");
	if (IS_ERR(emmc_a)) {
		dev_err(&pdev->dev, "could not get pin status, emmc_a\n");
		return PTR_ERR(emmc_a);
	}
	emmc_r = pinctrl_lookup_state(pctrl, "emmc_r");
	if (IS_ERR(emmc_r)) {
		dev_err(&pdev->dev, "could not get pin status, emmc_r\n");
		return PTR_ERR(emmc_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_emmc_mux_of_match[] = {
	{ .compatible = "bitmain,emmc-mux" },
	{ }
};
static struct platform_driver bm_emmc_mux_driver = {
	.driver = {
		.name = "bm-emmc-mux",
		.of_match_table = bm_emmc_mux_of_match,
	},
	.probe = bm_emmc_mux_probe,
};


static int __init bm_emmc_mux_init(void)
{
	return platform_driver_register(&bm_emmc_mux_driver);
}
arch_initcall(bm_emmc_mux_init);

//==UART==

static int bm_uart1_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart1\n");
	uart1_a = pinctrl_lookup_state(pctrl, "uart1_a");
	if (IS_ERR(uart1_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart1_a\n");
		return PTR_ERR(uart1_a);
	}
	uart1_r = pinctrl_lookup_state(pctrl, "uart1_r");
	if (IS_ERR(uart1_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart1_r\n");
		return PTR_ERR(uart1_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart1_mux_of_match[] = {
	{ .compatible = "bitmain,uart1-mux" },
	{ }
};
static struct platform_driver bm_uart1_mux_driver = {
	.driver = {
		.name = "bm-uart1-mux",
		.of_match_table = bm_uart1_mux_of_match,
	},
	.probe = bm_uart1_mux_probe,
};


static int __init bm_uart1_mux_init(void)
{
	return platform_driver_register(&bm_uart1_mux_driver);
}
arch_initcall(bm_uart1_mux_init);

//UART2
static int bm_uart2_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart2\n");
	uart2_a = pinctrl_lookup_state(pctrl, "uart2_a");
	if (IS_ERR(uart2_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart2_a\n");
		return PTR_ERR(uart2_a);
	}
	uart2_r = pinctrl_lookup_state(pctrl, "uart2_r");
	if (IS_ERR(uart2_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart2_r\n");
		return PTR_ERR(uart2_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart2_mux_of_match[] = {
	{ .compatible = "bitmain,uart2-mux" },
	{ }
};
static struct platform_driver bm_uart2_mux_driver = {
	.driver = {
		.name = "bm-uart2-mux",
		.of_match_table = bm_uart2_mux_of_match,
	},
	.probe = bm_uart2_mux_probe,
};


static int __init bm_uart2_mux_init(void)
{
	return platform_driver_register(&bm_uart2_mux_driver);
}
arch_initcall(bm_uart2_mux_init);
//UART3


static int bm_uart3_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart3\n");
	uart3_a = pinctrl_lookup_state(pctrl, "uart3_a");
	if (IS_ERR(uart3_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart3_a\n");
		return PTR_ERR(uart3_a);
	}
	uart3_r = pinctrl_lookup_state(pctrl, "uart3_r");
	if (IS_ERR(uart3_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart3_r\n");
		return PTR_ERR(uart3_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart3_mux_of_match[] = {
	{ .compatible = "bitmain,uart3-mux" },
	{ }
};
static struct platform_driver bm_uart3_mux_driver = {
	.driver = {
		.name = "bm-uart3-mux",
		.of_match_table = bm_uart3_mux_of_match,
	},
	.probe = bm_uart3_mux_probe,
};


static int __init bm_uart3_mux_init(void)
{
	return platform_driver_register(&bm_uart3_mux_driver);
}
arch_initcall(bm_uart3_mux_init);

//UART4

static int bm_uart4_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart4\n");
	uart4_a = pinctrl_lookup_state(pctrl, "uart4_a");
	if (IS_ERR(uart4_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart4_a\n");
		return PTR_ERR(uart4_a);
	}
	uart4_r = pinctrl_lookup_state(pctrl, "uart4_r");
	if (IS_ERR(uart4_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart4_r\n");
		return PTR_ERR(uart4_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart4_mux_of_match[] = {
	{ .compatible = "bitmain,uart4-mux" },
	{ }
};
static struct platform_driver bm_uart4_mux_driver = {
	.driver = {
		.name = "bm-uart4-mux",
		.of_match_table = bm_uart4_mux_of_match,
	},
	.probe = bm_uart4_mux_probe,
};


static int __init bm_uart4_mux_init(void)
{
	return platform_driver_register(&bm_uart4_mux_driver);
}
arch_initcall(bm_uart4_mux_init);

//UART5


static int bm_uart5_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart5\n");
	uart5_a = pinctrl_lookup_state(pctrl, "uart5_a");
	if (IS_ERR(uart5_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart5_a\n");
		return PTR_ERR(uart5_a);
	}
	uart5_r = pinctrl_lookup_state(pctrl, "uart5_r");
	if (IS_ERR(uart5_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart5_r\n");
		return PTR_ERR(uart5_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart5_mux_of_match[] = {
	{ .compatible = "bitmain,uart5-mux" },
	{ }
};
static struct platform_driver bm_uart5_mux_driver = {
	.driver = {
		.name = "bm-uart5-mux",
		.of_match_table = bm_uart5_mux_of_match,
	},
	.probe = bm_uart5_mux_probe,
};


static int __init bm_uart5_mux_init(void)
{
	return platform_driver_register(&bm_uart5_mux_driver);
}
arch_initcall(bm_uart5_mux_init);

//UART6


static int bm_uart6_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart6\n");
	uart6_a = pinctrl_lookup_state(pctrl, "uart6_a");
	if (IS_ERR(uart6_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart6_a\n");
		return PTR_ERR(uart6_a);
	}
	uart6_r = pinctrl_lookup_state(pctrl, "uart6_r");
	if (IS_ERR(uart6_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart6_r\n");
		return PTR_ERR(uart6_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart6_mux_of_match[] = {
	{ .compatible = "bitmain,uart6-mux" },
	{ }
};
static struct platform_driver bm_uart6_mux_driver = {
	.driver = {
		.name = "bm-uart6-mux",
		.of_match_table = bm_uart6_mux_of_match,
	},
	.probe = bm_uart6_mux_probe,
};


static int __init bm_uart6_mux_init(void)
{
	return platform_driver_register(&bm_uart6_mux_driver);
}
arch_initcall(bm_uart6_mux_init);

//UART7

static int bm_uart7_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart7\n");
	uart7_a = pinctrl_lookup_state(pctrl, "uart7_a");
	if (IS_ERR(uart7_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart7_a\n");
		return PTR_ERR(uart7_a);
	}
	uart7_r = pinctrl_lookup_state(pctrl, "uart7_r");
	if (IS_ERR(uart7_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart7_r\n");
		return PTR_ERR(uart7_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart7_mux_of_match[] = {
	{ .compatible = "bitmain,uart7-mux" },
	{ }
};
static struct platform_driver bm_uart7_mux_driver = {
	.driver = {
		.name = "bm-uart7-mux",
		.of_match_table = bm_uart7_mux_of_match,
	},
	.probe = bm_uart7_mux_probe,
};


static int __init bm_uart7_mux_init(void)
{
	return platform_driver_register(&bm_uart7_mux_driver);
}
arch_initcall(bm_uart7_mux_init);
//UART8

static int bm_uart8_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart8\n");
	uart8_a = pinctrl_lookup_state(pctrl, "uart8_a");
	if (IS_ERR(uart8_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart8_a\n");
		return PTR_ERR(uart8_a);
	}
	uart8_r = pinctrl_lookup_state(pctrl, "uart8_r");
	if (IS_ERR(uart8_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart8_r\n");
		return PTR_ERR(uart8_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart8_mux_of_match[] = {
	{ .compatible = "bitmain,uart8-mux" },
	{ }
};
static struct platform_driver bm_uart8_mux_driver = {
	.driver = {
		.name = "bm-uart8-mux",
		.of_match_table = bm_uart8_mux_of_match,
	},
	.probe = bm_uart8_mux_probe,
};


static int __init bm_uart8_mux_init(void)
{
	return platform_driver_register(&bm_uart8_mux_driver);
}
arch_initcall(bm_uart8_mux_init);
//UART9

static int bm_uart9_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart9\n");
	uart9_a = pinctrl_lookup_state(pctrl, "uart9_a");
	if (IS_ERR(uart9_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart9_a\n");
		return PTR_ERR(uart9_a);
	}
	uart9_r = pinctrl_lookup_state(pctrl, "uart9_r");
	if (IS_ERR(uart9_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart9_r\n");
		return PTR_ERR(uart9_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart9_mux_of_match[] = {
	{ .compatible = "bitmain,uart9-mux" },
	{ }
};
static struct platform_driver bm_uart9_mux_driver = {
	.driver = {
		.name = "bm-uart9-mux",
		.of_match_table = bm_uart9_mux_of_match,
	},
	.probe = bm_uart9_mux_probe,
};


static int __init bm_uart9_mux_init(void)
{
	return platform_driver_register(&bm_uart9_mux_driver);
}
arch_initcall(bm_uart9_mux_init);

//UART10

static int bm_uart10_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl;

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);

	// dev_info(&pdev->dev, "get pinctrl\n");
	pctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart10\n");
	uart10_a = pinctrl_lookup_state(pctrl, "uart10_a");
	if (IS_ERR(uart10_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart10_a\n");
		return PTR_ERR(uart10_a);
	}
	uart10_r = pinctrl_lookup_state(pctrl, "uart10_r");
	if (IS_ERR(uart10_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart10_r\n");
		return PTR_ERR(uart10_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart10_mux_of_match[] = {
	{ .compatible = "bitmain,uart10-mux" },
	{ }
};
static struct platform_driver bm_uart10_mux_driver = {
	.driver = {
		.name = "bm-uart10-mux",
		.of_match_table = bm_uart10_mux_of_match,
	},
	.probe = bm_uart10_mux_probe,
};


static int __init bm_uart10_mux_init(void)
{
	return platform_driver_register(&bm_uart10_mux_driver);
}
arch_initcall(bm_uart10_mux_init);

//UART11

static int bm_uart11_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "get pinstatus\n");

	// dev_info(&pdev->dev, "uart11\n");
	uart11_a = pinctrl_lookup_state(pctrl, "uart11_a");
	if (IS_ERR(uart11_a)) {
		dev_err(&pdev->dev, "could not get pin status, uart11_a\n");
		return PTR_ERR(uart11_a);
	}
	uart11_r = pinctrl_lookup_state(pctrl, "uart11_r");
	if (IS_ERR(uart11_r)) {
		dev_err(&pdev->dev, "could not get pin status, uart11_r\n");
		return PTR_ERR(uart11_r);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
	dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_uart11_mux_of_match[] = {
	{ .compatible = "bitmain,uart11-mux" },
	{ }
};
static struct platform_driver bm_uart11_mux_driver = {
	.driver = {
		.name = "bm-uart11-mux",
		.of_match_table = bm_uart11_mux_of_match,
	},
	.probe = bm_uart11_mux_probe,
};


static int __init bm_uart11_mux_init(void)
{
	return platform_driver_register(&bm_uart11_mux_driver);
}
arch_initcall(bm_uart11_mux_init);

//PWM


static int bm_pwm_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "pwm\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	//u32 *first_mux_addr = of_get_property(dev_mux->dev->of_node, "source_mux", NULL);
	//printk("first_mux_addr:%p %x\n",first_mux_addr, be32_to_cpup(first_mux_addr));
	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_pwm_mux_of_match[] = {
	{ .compatible = "bitmain,pwm-mux" },
	{ }
};
static struct platform_driver bm_pwm_mux_driver = {
	.driver = {
		.name = "bm-pwm-mux",
		.of_match_table = bm_pwm_mux_of_match,
	},
	.probe = bm_pwm_mux_probe,
};


static int __init bm_pwm_mux_init(void)
{
	return platform_driver_register(&bm_pwm_mux_driver);
}
arch_initcall(bm_pwm_mux_init);

//I2C MUX
static int bm_i2c_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "i2c\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");


//Set default pinmux to I2C
	// dev_info(&pdev->dev, "Set I2C pin MUX\n");
	pinctrl_select_state(pctrl, dev_mux->acquire);
//Set pinmux end

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_i2c_mux_of_match[] = {
	{ .compatible = "bitmain,i2c-mux" },
	{ }
};
static struct platform_driver bm_i2c_mux_driver = {
	.driver = {
		.name = "bm-i2c-mux",
		.of_match_table = bm_i2c_mux_of_match,
	},
	.probe = bm_i2c_mux_probe,
};


static int __init bm_i2c_mux_init(void)
{
	return platform_driver_register(&bm_i2c_mux_driver);
}
arch_initcall(bm_i2c_mux_init);

//ETH MUX
static int bm_eth_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "eth\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_eth_mux_of_match[] = {
	{ .compatible = "bitmain,eth-mux" },
	{ }
};
static struct platform_driver bm_eth_mux_driver = {
	.driver = {
		.name = "bm-eth-mux",
		.of_match_table = bm_eth_mux_of_match,
	},
	.probe = bm_eth_mux_probe,
};


static int __init bm_eth_mux_init(void)
{
	return platform_driver_register(&bm_eth_mux_driver);
}
arch_initcall(bm_eth_mux_init);

//I2S MUX
static int bm_i2s_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "i2s\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

//Set default pinmux to I2S
	// dev_info(&pdev->dev, "Set I2S pin MUX\n");
	pinctrl_select_state(pctrl, dev_mux->acquire);
//Set pinmux end

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_i2s_mux_of_match[] = {
	{ .compatible = "bitmain,i2s-mux" },
	{ }
};
static struct platform_driver bm_i2s_mux_driver = {
	.driver = {
		.name = "bm-i2s-mux",
		.of_match_table = bm_i2s_mux_of_match,
	},
	.probe = bm_i2s_mux_probe,
};

static int __init bm_i2s_mux_init(void)
{
	return platform_driver_register(&bm_i2s_mux_driver);
}
arch_initcall(bm_i2s_mux_init);

//wifi MUX
static int bm_wifi_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "wifi\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

//Set default pinmux to GPIO13
	// dev_info(&pdev->dev, "Set GPIO13 pin MUX\n");
	pinctrl_select_state(pctrl, dev_mux->acquire);
//Set pinmux end

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_wifi_mux_of_match[] = {
	{ .compatible = "bitmain,wifi-mux" },
	{ }
};
static struct platform_driver bm_wifi_mux_driver = {
	.driver = {
		.name = "bm-wifi-mux",
		.of_match_table = bm_wifi_mux_of_match,
	},
	.probe = bm_wifi_mux_probe,
};

static int __init bm_wifi_mux_init(void)
{
	return platform_driver_register(&bm_wifi_mux_driver);
}
arch_initcall(bm_wifi_mux_init);

//spi0 MUX
static int bm_spi0_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);

	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;
	dev_mux->dev = &pdev->dev;
	// dev_info(&pdev->dev, "%s\n", __func__);
	//gp_pinctrl_init(pdev);
	// dev_info(&pdev->dev, "get pinctrl\n");
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);
	// dev_info(&pdev->dev, "spi\n");
	dev_mux->acquire = pinctrl_lookup_state(pctrl, "acquire");
	if (IS_ERR(dev_mux->acquire)) {
		dev_err(&pdev->dev, "could not get pin status, acquire\n");
		return PTR_ERR(dev_mux->acquire);
	}
	dev_mux->release = pinctrl_lookup_state(pctrl, "release");
	if (IS_ERR(dev_mux->release)) {
		dev_err(&pdev->dev, "could not get pin status, release\n");
		return PTR_ERR(dev_mux->release);
	}

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

//Set default pinmux to spi0
	pinctrl_select_state(pctrl, dev_mux->acquire);
//Set pinmux end

	dev_info(&pdev->dev, "initialized Bitman pin MUX driver\n");
	return ret;

}

static const struct of_device_id bm_spi0_mux_of_match[] = {
	{ .compatible = "bitmain,spi0-mux" },
	{ }
};
static struct platform_driver bm_spi0_mux_driver = {
	.driver = {
		.name = "bm-spi0-mux",
		.of_match_table = bm_spi0_mux_of_match,
	},
	.probe = bm_spi0_mux_probe,
};

static int __init bm_spi0_mux_init(void)
{
	return platform_driver_register(&bm_spi0_mux_driver);
}
arch_initcall(bm_spi0_mux_init);

static int bm_gpio_mux_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct pinctrl *pctrl = devm_pinctrl_get(&pdev->dev);
	struct bm_dev_mux *dev_mux = devm_kzalloc(&pdev->dev, sizeof(struct bm_dev_mux), GFP_KERNEL);

	if (!dev_mux)
		return -ENOMEM;

	dev_mux->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, dev_mux);

	if (IS_ERR(pctrl))
		return PTR_ERR(pctrl);

	if (device_create_file(&pdev->dev, &dev_attr_bm_mux))
		dev_info(&pdev->dev, "Unable to createsysfs entry\n");

	pinctrl_select_state(pctrl, dev_mux->acquire);
	dev_info(&pdev->dev, "initialized Bitmain gpio MUX driver\n");

	return ret;
}

static const struct of_device_id bm_gpio_mux_of_match[] = {
	{ .compatible = "bitmain,gpio-mux" },
	{ }
};

static struct platform_driver bm_gpio_mux_driver = {
	.driver = {
		.name = "bm-gpio-mux",
		.of_match_table = bm_gpio_mux_of_match,
	},
	.probe = bm_gpio_mux_probe,
};

static int __init bm_gpio_mux_init(void)
{
	return platform_driver_register(&bm_gpio_mux_driver);
}
arch_initcall(bm_gpio_mux_init);
