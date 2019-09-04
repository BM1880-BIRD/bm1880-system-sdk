#ifndef _DISP_TIMING_H_
#define _DISP_TIMING_H_

enum sclr_panel {
	SCL_PANEL_TTL,
	SCL_PANEL_MAX,
};

struct sclr_disp_timing panels[] = {
	{
		.vsync_pol = false,
		.hsync_pol = true,
		.vtotal = 628,
		.htotal = 831,
		.vsync_start = 1,
		.vsync_end = 4,
		.vfde_start = 28,
		.vfde_end = 627,
		.vmde_start = 28,
		.vmde_end = 627,
		.hsync_start = 1,
		.hsync_end = 10,
		.hfde_start = 15,
		.hfde_end = 815,
		.hmde_start = 15,
		.hmde_end = 815,
	},
};

#endif // _BMD_SCL_H_

