/***************************************************
 * File:touch.c
 * OPLUS_FEATURE_TP_BASIC
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * TAG: BSP.TP.Init
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/regulator/consumer.h>
#include "oplus_touchscreen/tp_devices.h"
#include "oplus_touchscreen/touchpanel_common.h"
#include <soc/oplus/system/oplus_project.h>
#include <soc/oplus/device_info.h>
#include "touch.h"

#define MAX_LIMIT_DATA_LENGTH         100
extern char *saved_command_line;
/*if can not compile success, please update vendor/oplus_touchsreen*/
struct tp_dev_name tp_dev_names[] = {
     {TP_OFILM, "OFILM"},
     {TP_BIEL, "BIEL"},
     {TP_TRULY, "TRULY"},
     {TP_BOE, "BOE"},
     {TP_G2Y, "G2Y"},
     {TP_TPK, "TPK"},
     {TP_JDI, "JDI"},
     {TP_TIANMA, "TIANMA"},
     {TP_SAMSUNG, "SAMSUNG"},
     {TP_DSJM, "DSJM"},
     {TP_BOE_B8, "BOEB8"},
     {TP_UNKNOWN, "UNKNOWN"},
};
int g_tp_prj_id = 0;
int g_tp_dev_vendor = TP_UNKNOWN;
char *g_tp_ext_prj_name = NULL;

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

bool __init tp_judge_ic_match(char *tp_ic_name)
{
	int prj_id = 0;
	prj_id = get_project();

	pr_err("[TP] tp_ic_name = %s\n", tp_ic_name);

	switch(prj_id) {
	case 21091:
	case 21261:
	case 21262:
	case 21263:
	case 21265:
	case 21266:
	case 21267:
	case 21268:
	case 0x216C1:
	case 0x216C2:
	case 0x216C3:
	case 0x216C4:
	case 0x216C5:
	case 0x216CB:
	case 22071:
	case 22072:
		pr_info("[TP] case %d\n", prj_id);
		if (strstr(tp_ic_name, "ilitek,ili7807s") && (strstr(saved_command_line, "ili9883a_90hz_boe")
		|| strstr(saved_command_line, "mdss_dsi_ili7807s_90hz_hlt_video")
		|| strstr(saved_command_line, "mdss_dsi_ili7807s_hlt_fhd_90hz_video")
		|| strstr(saved_command_line, "mdss_dsi_ili9883c_90hz_boe_video")
		|| strstr(saved_command_line, "mdss_dsi_ili9883c_90hz_hlt_video"))) {
			pr_err("[TP] touch judge ic = ilitek,ili7807s\n");
			goto OUT;
		}
		if (strstr(tp_ic_name, "novatek,nf_nt36672c") && strstr(saved_command_line, "mdss_dsi_nt36672c_tianma_fhd_video")) {
			pr_err("[TP] touch judge ic = novatek,nf_nt36672c\n");
			goto OUT;
		}
		if (strstr(tp_ic_name, "novatek,nf_nt36672c") && strstr(saved_command_line, "mdss_dsi_216c1_nt36672c_90hz_tm_video")) {
			pr_err("[TP] 1touch judge ic = novatek,nf_nt36672c\n");
			goto OUT;
		}
		break;
	case 21029:
		pr_info("[TP] case %d\n", prj_id);
		if (strstr(tp_ic_name, "novatek,nf_nt36523") && ((strstr(saved_command_line, "mdss_dsi_nt36523b_60hz_djn_video")) ||
			strstr(saved_command_line, "mdss_dsi_nt36523w_60hz_inx_video"))) {
			pr_err("[TP] touch judge ic = novatek,nf_nt36523\n");
			goto OUT;
		}
		break;
	case 21030:
		pr_info("[TP] case %d\n", prj_id);
		if (strstr(tp_ic_name, "novatek,nf_nt36523") && ((strstr(saved_command_line, "mdss_dsi_nt36523b_60hz_djn_video")) ||
			strstr(saved_command_line, "mdss_dsi_nt36523w_60hz_inx_video"))) {
			pr_err("[TP] touch judge ic = novatek,nf_nt36523\n");
			goto OUT;
		}
		break;
	case 21311:
	case 21312:
	case 21313:
	case 22251:
	case 22252:
                pr_info("[TP] case %d\n", prj_id);
                if (strstr(tp_ic_name, "Goodix-gt9886") && strstr(saved_command_line, "mdss_s6e8fc1fe_samsung")) {
                        pr_err("[TP] touch judge ic = Goodix-gt9886");
                        return true;
                }
                if (strstr(tp_ic_name, "focaltech,fts") && strstr(saved_command_line, "s6e3fc3_samsung_cmd")) {
                        pr_err("[TP] touch judge ic = ft3518");
                        return true;
                }
		return false;
	case 0x2172F:
	case 21730:
	case 21731:
		pr_info("[TP] case %d, project messi\n", prj_id);
		if (strstr(tp_ic_name, "focaltech,fts")) {
			pr_err("[TP] touch judge ic = focaltech,ft3518u\n");
			goto OUT;
		}
		break;
	default:
		pr_err("[TP] Invalid project: %d, not support cmdline: %s\n", prj_id, saved_command_line);
		break;
	}

	pr_err("[TP] Lcd module not found, prj is %d, tp ic is %s\n", prj_id, tp_ic_name);
	return false;
OUT:
	return true;
}

bool  tp_judge_ic_match_commandline(struct panel_info *panel_data)
{
    int prj_id = 0;
    int i = 0;
    prj_id = get_project();
    pr_err("[TP] boot_command_line = %s \n", saved_command_line);
	pr_err("[TP] prj_id = [%d],panel_data->project_num = [%d]\n", prj_id, panel_data->project_num);
    for(i = 0; i<panel_data->project_num; i++){
        if(prj_id == panel_data->platform_support_project[i]){
            g_tp_prj_id = panel_data->platform_support_project_dir[i];
            g_tp_ext_prj_name = panel_data->platform_support_external_name[i];
			pr_err("[TP] platform_support_commandline = %s \n", panel_data->platform_support_commandline[i]);
            if(strstr(saved_command_line, panel_data->platform_support_commandline[i])||strstr("default_commandline", panel_data->platform_support_commandline[i]) ){
                pr_err("[TP] Driver match the project\n");
                return true;
            }
        }
    }
    pr_err("[TP] Driver does not match the project\n");
    pr_err("Lcd module not found\n");
    return false;
}


int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    char* vendor;
    int prj_id = 0;

    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }

    prj_id = g_tp_prj_id;

	if (panel_data->manufacture_info.version) {
		memcpy(panel_data->manufacture_info.version, "0x", sizeof("0x"));

		if (g_tp_ext_prj_name) {
			strncpy(panel_data->manufacture_info.version + strlen(panel_data->manufacture_info.version),
					g_tp_ext_prj_name, 7);
		}
	}
    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    vendor = GET_TP_DEV_NAME(panel_data->tp_type);

    strcpy(panel_data->manufacture_info.manufacture, vendor);
    snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
            "tp/%d/FW_%s_%s.img",
            prj_id, panel_data->chip_name, vendor);

    if (panel_data->test_limit_name) {
        snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
            "tp/%d/LIMIT_%s_%s.img",
            prj_id, panel_data->chip_name, vendor);
    }

        if ((prj_id == 20221) || (prj_id == 20222) || (prj_id == 20223) || (prj_id == 20224) || (prj_id == 20225) || (prj_id == 20226)
		|| (prj_id == 20227) || (prj_id == 20228) || (prj_id == 20229) || (prj_id == 20021) || (prj_id == 20202) || (prj_id == 20203)
		|| (prj_id == 20204) || (prj_id == 20207) || (prj_id == 20208) || (prj_id == 21029) || (prj_id == 21030)) {
                pr_err("[TP]project is soda\n");
                pr_err("[TP] saved_command_line = [%s] \n", saved_command_line);
                if (strstr(saved_command_line, "mdss_dsi_ili9881h_90hz_boe_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_ILI9881H_90HZ_BOE.bin");
                        pr_err("[TP]This is ILI9881H_90HZ_BOE\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_ILI9881H_90HZ_BOE.ini");
                        }
                        panel_data->vid_len = 8;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218BI9", 8);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "90HZ_BOE", 8);
                        panel_data->firmware_headfile.firmware_data = FW_20221_ILI9881H_90HZ_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_ILI9881H_90HZ_BOE);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_90hz_hlt_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_ILI9881H_90HZ_HLT.bin");
                        pr_err("[TP]This is ILI9881H_90HZ_HLT\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_ILI9881H_90HZ_HLT.ini");
                        }
                        panel_data->vid_len = 8;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218HI9", 8);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "90HZ_HLT", 8);
                        panel_data->firmware_headfile.firmware_data = FW_20221_ILI9881H_90HZ_HLT;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_ILI9881H_90HZ_HLT);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_90hz_txd_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_ILI9882N_90HZ_TXD.bin");
                        pr_err("[TP]This is ILI9882N_TXD\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_ILI9882N_90HZ_TXD.ini");
                        }
                        panel_data->vid_len = 9;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218TI9N", 9);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "90HZ_TXD", 8);
                        panel_data->firmware_headfile.firmware_data = FW_20221_ILI9882N_TXD;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_ILI9882N_TXD);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_90hz_boe_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_ILI9882N_90HZ_BOE.bin");
                        pr_err("[TP]This is ILI9882N_BOE\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_ILI9882N_90HZ_BOE.ini");
                        }
                        panel_data->vid_len = 9;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218BI9N", 9);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "90HZ_BOE", 8);
                        panel_data->firmware_headfile.firmware_data = FW_20221_ILI9882N_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_ILI9882N_BOE);
                } else if (strstr(saved_command_line, "mdss_dsi_nt36523b_60hz_djn_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/21029/FW_NF_NT36523_60HZ_DJN.bin");
                        pr_err("[TP]This is NT36523_DJN\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/21029/LIMIT_NT36523_DJN.img");
                        }
                        panel_data->vid_len = 9;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "X21N2ND9N", 9);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "60HZ_DJN", 8);
                        panel_data->firmware_headfile.firmware_data = FW_21029_NT36523_DJN;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_21029_NT36523_DJN);
                } else if (strstr(saved_command_line, "mdss_dsi_nt36523w_60hz_inx_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/21029/FW_NF_NT36523_60HZ_INX.bin");
                        pr_err("[TP]This is NT36523_INX\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/21029/LIMIT_NT36523_INX.img");
                        }
                        panel_data->vid_len = 9;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "X21N2NI9N", 9);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "60HZ_INX", 8);
                        panel_data->firmware_headfile.firmware_data = FW_21029_NT36523_INX;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_21029_NT36523_INX);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_90hz_inx_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_ILI9882N_90HZ_INX.bin");
                        pr_err("[TP]This is ILI9882N_INX\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_ILI9882N_90HZ_INX.ini");
                        }
                        panel_data->vid_len = 9;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218II9N", 9);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "90HZ_INX", 8);
                        panel_data->firmware_headfile.firmware_data = FW_20221_ILI9882N_INX;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_ILI9882N_INX);
                } else if (strstr(saved_command_line, "mdss_dsi_nt36525b_90hz_inx_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20221/FW_NF_NT36525B_90HZ_INNOLUX.bin");
                        pr_err("[TP]This is NT36525B_90HZ_INNOLUX\n");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20221/LIMIT_NF_NT36525B_90HZ_INNOLUX.img");
                        }
                        panel_data->vid_len = 8;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "FA218IN9", 8);
						}
                        memcpy(panel_data->manufacture_info.manufacture, "INNOLUX", 7);
                        panel_data->firmware_headfile.firmware_data = FW_20221_NT36525B_90HZ_INNOLUX;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20221_NT36525B_90HZ_INNOLUX);
                }
        }

		if ((prj_id == 20211) || (prj_id == 20212) || (prj_id == 20213) || (prj_id == 20214) || (prj_id == 20215)) {
                pr_err("[TP]project is rum\n");
                snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20211/FW_GT9886_SAMSUNG.bin");
                if (panel_data->test_limit_name) {
					snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20211/LIMIT_GT9886_SAMSUNG.img");
                }
                panel_data->vid_len = 7;
				if (panel_data->manufacture_info.version) {
					memcpy(panel_data->manufacture_info.version, "FA218ON", 7);
				}
        }

        if ((prj_id == 20673) || (prj_id == 20674) || (prj_id == 20675) || (prj_id == 20677) || (prj_id == 0x2067D) || (prj_id == 0x2067E)) {
                printk("project is coco\n");
                if (strstr(saved_command_line, "mdss_dsi_hx83112a_tm_90hz_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20673/FW_NF_HX83112A_TIANMA.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20673/LIMIT_NF_HX83112A_TIANMA.img");
                        }
                        panel_data->vid_len = 15;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378_TM_Himax_", 15);
						}
                        panel_data->firmware_headfile.firmware_data = FW_20671_Hx83221A_TIANMA;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20671_Hx83221A_TIANMA);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_boe_90hz_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20673/FW_NF_ILI9881H_90HZ_BOE.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20673/LIMIT_NF_ILI9881H_90HZ_BOE.ini");
                        }
                        panel_data->vid_len = 14;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378_BOE_ILI_", 14);
						}
                        panel_data->firmware_headfile.firmware_data = FW_20673_ILI9881H_90HZ_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20673_ILI9881H_90HZ_BOE);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_hlt_90hz_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20673/FW_NF_ILI9881H_90HZ_HLT.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20673/LIMIT_NF_ILI9881H_90HZ_HLT.ini");
                        }
                        panel_data->vid_len = 14;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378_HLT_ILI_", 14);
						}
                        panel_data->firmware_headfile.firmware_data = FW_20673_ILI9881H_90HZ_HLT;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20673_ILI9881H_90HZ_HLT);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_boe_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20673/FW_NF_ILI9881H_BOE.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20673/LIMIT_NF_ILI9881H_BOE.ini");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378BI_", 8);
						}
                        panel_data->firmware_headfile.firmware_data = FW_20673_ILI9881H_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20673_ILI9881H_BOE);
                }
        }
        if ((prj_id == 20670) || (prj_id == 20671) || (prj_id == 20672) || (prj_id == 20676) || (prj_id == 20679) || (prj_id == 0x2067C)) {
                printk("project is coco-b\n");
                if (strstr(saved_command_line, "mdss_dsi_ili9881h_boe_90hz_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20671/FW_B_NF_ILI9881H_90HZ_BOE.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20671/LIMIT_B_NF_ILI9881H_90HZ_BOE.ini");
                        }
                        panel_data->vid_len = 16;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378_B_BOE_ILI_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_B_20671_ILI9881H_90HZ_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_B_20671_ILI9881H_90HZ_BOE);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_hlt_90hz_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20671/FW_B_NF_ILI9881H_90HZ_HLT.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20671/LIMIT_B_NF_ILI9881H_90HZ_HLT.ini");
                        }
                        panel_data->vid_len = 16;
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378_B_HLT_ILI_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_B_20671_ILI9881H_90HZ_HLT;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_B_20671_ILI9881H_90HZ_HLT);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9881h_boe_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/20671/FW_B_NF_ILI9881H_BOE.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/20671/LIMIT_B_NF_ILI9881H_BOE.ini");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA378BI_", 8);
						}
                        panel_data->firmware_headfile.firmware_data = FW_20673_ILI9881H_BOE;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_20673_ILI9881H_BOE);
                }
        }
        if ((prj_id == 0x206BD) || (prj_id == 0x206BE) || (prj_id == 0x206BF) || (prj_id == 0x206C0) ||
	        (prj_id == 0x206C1) || (prj_id == 0x206C2) || (prj_id == 0x206C3)) {
                printk("project is pascal-h\n");
                if (strstr(saved_command_line, "mdss_dsi_nt36525b_hlt_b3_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_NT36525B_BOE_B3.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_NT36525B_BOE_B3.img");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_HLT_B3_NT_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_NT36525B_BOE_B3;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_NT36525B_BOE_B3);
                } else if (strstr(saved_command_line, "mdss_dsi_nt36525b_hlt_b8_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_NT36525B_BOE_B8.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_NT36525B_BOE_B8.img");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_HLT_B8_NT_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_NT36525B_BOE_B8;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_NT36525B_BOE_B8);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_cdot_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_ILI9882N_CDOT.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_ILI9882N_CDOT.ini");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_CDO_AU_IL_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_ILI9882N_CDOT;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_ILI9882N_CDOT);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_truly_25_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_ILI9882N_TRULY.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_ILI9882N_TRULY.ini");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_TRU_25_IL_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_NT36525B_TRULY;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_NT36525B_TRULY);
                } else if (strstr(saved_command_line, "mdss_dsi_ili9882n_truly_26_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_ILI9882N_TRULY.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_ILI9882N_TRULY.ini");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_TRU_26_IL_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_NT36525B_TRULY;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_NT36525B_TRULY);
                } else if (strstr(saved_command_line, "mdss_dsi_nt36525b_inx_video")) {
                        snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH, "tp/206BD/FW_NF_NT36525B_INNOLUX.bin");
                        if (panel_data->test_limit_name) {
                                snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH, "tp/206BD/LIMIT_NF_NT36525B_INNOLUX.img");
                        }
						if (panel_data->manufacture_info.version) {
							memcpy(panel_data->manufacture_info.version, "RA511_INX_T2_NT_", 16);
						}
                        panel_data->firmware_headfile.firmware_data = FW_206BD_NT36525B_INNOLUX;
                        panel_data->firmware_headfile.firmware_size = sizeof(FW_206BD_NT36525B_INNOLUX);
                }
        }
	if ((prj_id == 0x2172F) || (prj_id == 21730) || (prj_id == 21731)) {
		pr_info("project is messi\n");
		if (panel_data->manufacture_info.version) {
			memcpy(panel_data->manufacture_info.version, "FT3518_", 7);
		}
		snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
			"tp/2172F/FW_%s_%s.img",
			panel_data->chip_name, vendor);

		if (panel_data->test_limit_name) {
			snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
			"tp/2172F/LIMIT_%s_%s.img",
			panel_data->chip_name, vendor);
		}
		panel_data->manufacture_info.fw_path = panel_data->fw_name;
	}

	if ((prj_id == 21261) || (prj_id == 21262) || (prj_id == 21263) || (prj_id == 21265)
	|| (prj_id == 21266) || (prj_id == 21267) || (prj_id == 21268) || (prj_id == 21091)) {
		if (strstr(saved_command_line, "mdss_dsi_td4330_truly_v2")
		|| strstr(saved_command_line, "mdss_dsi_nt36672c_tianma_fhd_video")
		|| strstr(saved_command_line, "default_commandline")) {
			pr_err("[TP]project is golf-FHD\n");
			if (panel_data->manufacture_info.version) {
				memcpy(panel_data->manufacture_info.version, "AA256TN00", 9);
			}
			panel_data->firmware_headfile.firmware_data = FW_21261_NT36672C_TIANMA;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_21261_NT36672C_TIANMA);
		} else if (strstr(saved_command_line, "mdss_dsi_ili7807s_120hz_hlt_video")
			|| strstr(saved_command_line, "mdss_dsi_ili7807s_hlt_fhd_90hz_video")
			|| strstr(saved_command_line, "mdss_dsi_ili7807s_90hz_hlt_video")
			|| strstr(saved_command_line, "default_commandline")) {
			panel_data->chip_name = "NF_ILI7807S";
			vendor = "HLT";
			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/%d/FW_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/%d/LIMIT_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);
			}
			strcpy(panel_data->manufacture_info.manufacture, vendor);

			if (panel_data->manufacture_info.version) {
				memcpy(panel_data->manufacture_info.version, "AA256HI00", 9);
			}
			panel_data->firmware_headfile.firmware_data = FW_21261_ILI7807S_HLT;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_21261_ILI7807S_HLT);
			/*
			panel_data->firmware_headfile.firmware_data = FW_21261_ILI9883A_BOE;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_21261_ILI9883A_BOE);
			*/
		} else if (strstr(saved_command_line, "ili9883a_90hz_boe") || strstr(saved_command_line, "default_commandline")) {
			panel_data->chip_name = "NF_ILI9883A";
			vendor = "BOE";
			hw_res->TX_NUM = 18;
			hw_res->RX_NUM = 32;
			pr_err("[TP] : golf-HD is %s-%s and (%d, %d)",
				panel_data->chip_name, vendor, hw_res->TX_NUM, hw_res->RX_NUM);

			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/%d/FW_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/%d/LIMIT_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);
			}

			strcpy(panel_data->manufacture_info.manufacture, vendor);

			if (panel_data->manufacture_info.version) {
				memcpy(panel_data->manufacture_info.version, "AA256BI00", 9);
			}
			panel_data->firmware_headfile.firmware_data = FW_21261_ILI9883A_BOE;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_21261_ILI9883A_BOE);
		}
	}
	if (prj_id == 0x216C1) {
		if (strstr(saved_command_line, "mdss_dsi_nt36672c_tianma_fhd_video") || strstr(saved_command_line, "mdss_dsi_216c1_nt36672c_90hz_tm_video")) {
			pr_err("[TP]project is kasa-FHD\n");
			if (prj_id == 0x216C1) {
				memcpy(panel_data->manufacture_info.version, "AA256_TM_NT_", sizeof("AA256_TM_NT_"));
				panel_data->firmware_headfile.firmware_data = FW_216C1_NT36672C_TIANMA;
				panel_data->firmware_headfile.firmware_size = sizeof(FW_216C1_NT36672C_TIANMA);
			}
		} else if (strstr(saved_command_line, "mdss_dsi_ili7807s_90hz_hlt_video") ||
			strstr(saved_command_line, "mdss_dsi_ili7807s_hlt_fhd_90hz_video")) {
			panel_data->chip_name = "NF_ILI7807S";
			vendor = "HLT";
			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/%d/FW_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/%d/LIMIT_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);
			}
			strcpy(panel_data->manufacture_info.manufacture, vendor);

			if (prj_id == 0x216C1) {
				memcpy(panel_data->manufacture_info.version, "AA256_HLT_ILI_", 14);
				panel_data->firmware_headfile.firmware_data = FW_216C1_ILI7807S_HLT;
				panel_data->firmware_headfile.firmware_size = sizeof(FW_216C1_ILI7807S_HLT);
			}

		} else if (strstr(saved_command_line, "ili9883a_90hz_boe")) {
			panel_data->chip_name = "NF_ILI9883A";
			vendor = "BOE";
			hw_res->TX_NUM = 18;
			hw_res->RX_NUM = 32;
			pr_err("[TP] : golf-HD is %s-%s and (%d, %d)",
				panel_data->chip_name, vendor, hw_res->TX_NUM, hw_res->RX_NUM);

			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/%d/FW_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/%d/LIMIT_%s_%s.img",
				prj_id, panel_data->chip_name, vendor);
			}
			strcpy(panel_data->manufacture_info.manufacture, vendor);
			memcpy(panel_data->manufacture_info.version, "0xAA256BI", 9);
		}
	}

		if ((prj_id == 21311) || (prj_id == 21312) || (prj_id == 21313)) {
			if (panel_data->manufacture_info.version) {
				memcpy(panel_data->manufacture_info.version, "0xAA3110000", 11);
			}
	}

	if ((prj_id == 22071) || (prj_id == 22072)) {
		if (strstr(saved_command_line, "mdss_dsi_ili9883c_90hz_boe_video")) {
			panel_data->chip_name = "NF_ILI9883C";
			vendor = "BOE";
			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/22071/FW_%s_%s.img",
				panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/22071/LIMIT_%s_%s.img",
				panel_data->chip_name, vendor);
			}
			strcpy(panel_data->manufacture_info.manufacture, vendor);
			memcpy(panel_data->manufacture_info.version, "0xAC022BI", 9);
			panel_data->firmware_headfile.firmware_data = FW_22071_ILI9883C_BOE;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_22071_ILI9883C_BOE);
		} else if (strstr(saved_command_line, "mdss_dsi_ili9883c_90hz_hlt_video")) {
			panel_data->chip_name = "NF_ILI9883C";
			vendor = "HLT";
			snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
				"tp/22071/FW_%s_%s.img",
				panel_data->chip_name, vendor);

			if (panel_data->test_limit_name) {
				snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
				"tp/22071/LIMIT_%s_%s.img",
				panel_data->chip_name, vendor);
			}
			strcpy(panel_data->manufacture_info.manufacture, vendor);
			memcpy(panel_data->manufacture_info.version, "0xAC022HI", 9);
			panel_data->firmware_headfile.firmware_data = FW_22071_ILI9883C_HLT;
			panel_data->firmware_headfile.firmware_size = sizeof(FW_22071_ILI9883C_HLT);
		}
	}

    panel_data->manufacture_info.fw_path = panel_data->fw_name;

    pr_info("[TP]vendor:%s fw:%s limit:%s\n",
        vendor,
        panel_data->fw_name,
        panel_data->test_limit_name==NULL?"NO Limit":panel_data->test_limit_name);
    return 0;
}

int preconfig_power_control(struct touchpanel_data *ts)
{
	int prj_id = 0;

	prj_id = g_tp_prj_id;

	if (prj_id == 21261) {
		if (strstr(saved_command_line, "ili9883a_90hz_boe")) {
			ts->resolution_info.max_x = 720;
			ts->resolution_info.max_y = 1612;
			ts->resolution_info.LCD_WIDTH = 720;
			ts->resolution_info.LCD_HEIGHT = 1612;
			pr_err("[TP] golf-HD is %d, %d", ts->resolution_info.max_x, ts->resolution_info.max_y);
		}
	}

	return 0;
}
EXPORT_SYMBOL(preconfig_power_control);

int reconfig_power_control(struct touchpanel_data *ts)
{
    return 0;
}
EXPORT_SYMBOL(reconfig_power_control);

