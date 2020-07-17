/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * Dragino_gw_fwd -- An opensource lora gateway forward 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*!
 * \file
 * \brief gateway configure, parse json file
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "fwd.h"
#include "parson.h"
#include "loragw_hal.h"
#include "loragw_gps.h"
#include "loragw_aux.h"
#include "loragw_reg.h"
#include "loragw_debug.h"

DECLARE_GW;

static int parse_SX130x_configuration(const char* conf_file, gw_s* gw) {
    int i, j;
    char param_name[32]; /* used to generate variable parameter names */
    const char *str; /* used to store string value from JSON object */
    const char conf_obj_name[] = "SX130x_conf";
    JSON_Value *root_val = NULL;
    JSON_Value *val = NULL;
    JSON_Object *conf_obj = NULL;
    JSON_Object *conf_txgain_obj;
    JSON_Object *conf_ts_obj;
    JSON_Array *conf_txlut_array;

    struct lgw_conf_board_s boardconf;
    struct lgw_conf_rxrf_s rfconf;
    struct lgw_conf_rxif_s ifconf;
    struct lgw_conf_timestamp_s tsconf;
    uint32_t sf, bw, fdev;
    bool sx1250_tx_lut;

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        printf("ERROR: %s is not a valid JSON file\n", conf_file);
        return -1;
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        printf("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        printf("INFO: %s does contain a JSON object named %s, parsing SX1302 parameters\n", conf_file, conf_obj_name);
    }

    /* set board configuration */
    memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
    str = json_object_get_string(conf_obj, "spidev_path");
    if (str != NULL) {
        strncpy(boardconf.spidev_path, str, sizeof boardconf.spidev_path);
        boardconf.spidev_path[sizeof boardconf.spidev_path - 1] = '\0'; /* ensure string termination */
    } else {
        printf("ERROR: spidev path must be configured in %s\n", conf_file);
        return -1;
    }

    val = json_object_get_value(conf_obj, "lorawan_public"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONBoolean) {
        boardconf.lorawan_public = (bool)json_value_get_boolean(val);
    } else {
        printf("WARNING: Data type for lorawan_public seems wrong, please check\n");
        boardconf.lorawan_public = false;
    }
    val = json_object_get_value(conf_obj, "clksrc"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONNumber) {
        boardconf.clksrc = (uint8_t)json_value_get_number(val);
    } else {
        printf("WARNING: Data type for clksrc seems wrong, please check\n");
        boardconf.clksrc = 0;
    }
    val = json_object_get_value(conf_obj, "full_duplex"); /* fetch value (if possible) */
    if (json_value_get_type(val) == JSONBoolean) {
        boardconf.full_duplex = (bool)json_value_get_boolean(val);
    } else {
        printf("WARNING: Data type for full_duplex seems wrong, please check\n");
        boardconf.full_duplex = false;
    }
    printf("INFO: spidev_path %s, lorawan_public %d, clksrc %d, full_duplex %d\n", boardconf.spidev_path, boardconf.lorawan_public, boardconf.clksrc, boardconf.full_duplex);
    /* all parameters parsed, submitting configuration to the HAL */
    if (lgw_board_setconf(&boardconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: Failed to configure board\n");
        return -1;
    }

    /* set antenna gain configuration */
    val = json_object_get_value(conf_obj, "antenna_gain"); /* fetch value (if possible) */
    if (val != NULL) {
        if (json_value_get_type(val) == JSONNumber) {
            GW.hal.antenna_gain = (int8_t)json_value_get_number(val);
        } else {
            printf("WARNING: Data type for antenna_gain seems wrong, please check\n");
            GW.hal.antenna_gain = 0;
        }
    }
    printf("INFO: antenna_gain %d dBi\n", GW.hal.antenna_gain);

    /* set timestamp configuration */
    conf_ts_obj = json_object_get_object(conf_obj, "precision_timestamp");
    if (conf_ts_obj == NULL) {
        printf("INFO: %s does not contain a JSON object for precision timestamp\n", conf_file);
    } else {
        val = json_object_get_value(conf_ts_obj, "enable"); /* fetch value (if possible) */
        if (json_value_get_type(val) == JSONBoolean) {
            tsconf.enable_precision_ts = (bool)json_value_get_boolean(val);
        } else {
            printf("WARNING: Data type for precision_timestamp.enable seems wrong, please check\n");
            tsconf.enable_precision_ts = false;
        }
        if (tsconf.enable_precision_ts == true) {
            val = json_object_get_value(conf_ts_obj, "max_ts_metrics"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                tsconf.max_ts_metrics = (uint8_t)json_value_get_number(val);
            } else {
                printf("WARNING: Data type for precision_timestamp.max_ts_metrics seems wrong, please check\n");
                tsconf.max_ts_metrics = 0xFF;
            }
            val = json_object_get_value(conf_ts_obj, "nb_symbols"); /* fetch value (if possible) */
            if (json_value_get_type(val) == JSONNumber) {
                tsconf.nb_symbols = (uint8_t)json_value_get_number(val);
            } else {
                printf("WARNING: Data type for precision_timestamp.nb_symbols seems wrong, please check\n");
                tsconf.nb_symbols = 1;
            }
            printf("INFO: Configuring precision timestamp: max_ts_metrics:%u, nb_symbols:%u\n", tsconf.max_ts_metrics, tsconf.nb_symbols);

            /* all parameters parsed, submitting configuration to the HAL */
            if (lgw_timestamp_setconf(&tsconf) != LGW_HAL_SUCCESS) {
                printf("ERROR: Failed to configure precision timestamp\n");
                return -1;
            }
        } else {
            printf("INFO: Configuring legacy timestamp\n");
        }
    }

    /* set configuration for RF chains */
    for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
        memset(&rfconf, 0, sizeof rfconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "radio_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            printf("INFO: no configuration for radio %i\n", i);
            continue;
        }
        /* there is an object to configure that radio, let's parse it */
        snprintf(param_name, sizeof param_name, "radio_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            rfconf.enable = (bool)json_value_get_boolean(val);
        } else {
            rfconf.enable = false;
        }
        if (rfconf.enable == false) { /* radio disabled, nothing else to parse */
            printf("INFO: radio %i disabled\n", i);
        } else  { /* radio enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "radio_%i.freq", i);
            rfconf.freq_hz = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_offset", i);
            rfconf.rssi_offset = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_tcomp.coeff_a", i);
            rfconf.rssi_tcomp.coeff_a = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_tcomp.coeff_b", i);
            rfconf.rssi_tcomp.coeff_b = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_tcomp.coeff_c", i);
            rfconf.rssi_tcomp.coeff_c = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_tcomp.coeff_d", i);
            rfconf.rssi_tcomp.coeff_d = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.rssi_tcomp.coeff_e", i);
            rfconf.rssi_tcomp.coeff_e = (float)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "radio_%i.type", i);
            str = json_object_dotget_string(conf_obj, param_name);
            if (!strncmp(str, "SX1255", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1255;
            } else if (!strncmp(str, "SX1257", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1257;
            } else if (!strncmp(str, "SX1250", 6)) {
                rfconf.type = LGW_RADIO_TYPE_SX1250;
            } else {
                printf("WARNING: invalid radio type: %s (should be SX1255 or SX1257 or SX1250)\n", str);
            }
            snprintf(param_name, sizeof param_name, "radio_%i.single_input_mode", i);
            val = json_object_dotget_value(conf_obj, param_name);
            if (json_value_get_type(val) == JSONBoolean) {
                rfconf.single_input_mode = (bool)json_value_get_boolean(val);
            } else {
                rfconf.single_input_mode = false;
            }

            snprintf(param_name, sizeof param_name, "radio_%i.tx_enable", i);
            val = json_object_dotget_value(conf_obj, param_name);
            if (json_value_get_type(val) == JSONBoolean) {
                rfconf.tx_enable = (bool)json_value_get_boolean(val);
                if (rfconf.tx_enable == true) {
                    /* tx is enabled on this rf chain, we need its frequency range */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_min", i);
                    gw->tx.tx_freq_min[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_freq_max", i);
                    gw->tx.tx_freq_max[i] = (uint32_t)json_object_dotget_number(conf_obj, param_name);
                    if ((gw->tx.tx_freq_min[i] == 0) || (gw->tx.tx_freq_max[i] == 0)) {
                        printf("WARNING: no frequency range specified for TX rf chain %d\n", i);
                    }

                    /* set configuration for tx gains */
                    memset(&gw->tx.txlut[i], 0, sizeof gw->tx.txlut[i]); /* initialize configuration structure */
                    snprintf(param_name, sizeof param_name, "radio_%i.tx_gain_lut", i);
                    conf_txlut_array = json_object_dotget_array(conf_obj, param_name);
                    if (conf_txlut_array != NULL) {
                        gw->tx.txlut[i].size = json_array_get_count(conf_txlut_array);
                        /* Detect if we have a sx125x or sx1250 configuration */
                        conf_txgain_obj = json_array_get_object(conf_txlut_array, 0);
                        val = json_object_dotget_value(conf_txgain_obj, "pwr_idx");
                        if (val != NULL) {
                            printf("INFO: Configuring Tx Gain LUT for rf_chain %u with %u indexes for sx1250\n", i, gw->tx.txlut[i].size);
                            sx1250_tx_lut = true;
                        } else {
                            printf("INFO: Configuring Tx Gain LUT for rf_chain %u with %u indexes for sx125x\n", i, gw->tx.txlut[i].size);
                            sx1250_tx_lut = false;
                        }
                        /* Parse the table */
                        for (j = 0; j < (int)gw->tx.txlut[i].size; j++) {
                             /* Sanity check */
                            if (j >= TX_GAIN_LUT_SIZE_MAX) {
                                printf("ERROR: TX Gain LUT [%u] index %d not supported, skip it\n", i, j);
                                break;
                            }
                            /* Get TX gain object from LUT */
                            conf_txgain_obj = json_array_get_object(conf_txlut_array, j);
                            /* rf power */
                            val = json_object_dotget_value(conf_txgain_obj, "rf_power");
                            if (json_value_get_type(val) == JSONNumber) {
                                gw->tx.txlut[i].lut[j].rf_power = (int8_t)json_value_get_number(val);
                            } else {
                                printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "rf_power", j);
                                gw->tx.txlut[i].lut[j].rf_power = 0;
                            }
                            /* PA gain */
                            val = json_object_dotget_value(conf_txgain_obj, "pa_gain");
                            if (json_value_get_type(val) == JSONNumber) {
                                gw->tx.txlut[i].lut[j].pa_gain = (uint8_t)json_value_get_number(val);
                            } else {
                                printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "pa_gain", j);
                                gw->tx.txlut[i].lut[j].pa_gain = 0;
                            }
                            if (sx1250_tx_lut == false) {
                                /* DIG gain */
                                val = json_object_dotget_value(conf_txgain_obj, "dig_gain");
                                if (json_value_get_type(val) == JSONNumber) {
                                    gw->tx.txlut[i].lut[j].dig_gain = (uint8_t)json_value_get_number(val);
                                } else {
                                    printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "dig_gain", j);
                                    gw->tx.txlut[i].lut[j].dig_gain = 0;
                                }
                                /* DAC gain */
                                val = json_object_dotget_value(conf_txgain_obj, "dac_gain");
                                if (json_value_get_type(val) == JSONNumber) {
                                    gw->tx.txlut[i].lut[j].dac_gain = (uint8_t)json_value_get_number(val);
                                } else {
                                    printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "dac_gain", j);
                                    gw->tx.txlut[i].lut[j].dac_gain = 3; /* This is the only dac_gain supported for now */
                                }
                                /* MIX gain */
                                val = json_object_dotget_value(conf_txgain_obj, "mix_gain");
                                if (json_value_get_type(val) == JSONNumber) {
                                    gw->tx.txlut[i].lut[j].mix_gain = (uint8_t)json_value_get_number(val);
                                } else {
                                    printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "mix_gain", j);
                                    gw->tx.txlut[i].lut[j].mix_gain = 0;
                                }
                            } else {
                                /* TODO: rework this, should not be needed for sx1250 */
                                gw->tx.txlut[i].lut[j].mix_gain = 5;

                                /* power index */
                                val = json_object_dotget_value(conf_txgain_obj, "pwr_idx");
                                if (json_value_get_type(val) == JSONNumber) {
                                    gw->tx.txlut[i].lut[j].pwr_idx = (uint8_t)json_value_get_number(val);
                                } else {
                                    printf("WARNING: Data type for %s[%d] seems wrong, please check\n", "pwr_idx", j);
                                    gw->tx.txlut[i].lut[j].pwr_idx = 0;
                                }
                            }
                        }
                        /* all parameters parsed, submitting configuration to the HAL */
                        if (gw->tx.txlut[i].size > 0) {
                            if (lgw_txgain_setconf(i, &gw->tx.txlut[i]) != LGW_HAL_SUCCESS) {
                                printf("ERROR: Failed to configure concentrator TX Gain LUT for rf_chain %u\n", i);
                                return -1;
                            }
                        } else {
                            printf("WARNING: No TX gain LUT defined for rf_chain %u\n", i);
                        }
                    } else {
                        printf("WARNING: No TX gain LUT defined for rf_chain %u\n", i);
                    }
                }
            } else {
                rfconf.tx_enable = false;
            }
            printf("INFO: radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d, single input mode %d\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable, rfconf.single_input_mode);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxrf_setconf(i, &rfconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for radio %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
    for (i = 0; i < LGW_MULTI_NB; ++i) {
        memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i", i); /* compose parameter path inside JSON structure */
        val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
        if (json_value_get_type(val) != JSONObject) {
            printf("INFO: no configuration for Lora multi-SF channel %i\n", i);
            continue;
        }
        /* there is an object to configure that Lora multi-SF channel, let's parse it */
        snprintf(param_name, sizeof param_name, "chan_multiSF_%i.enable", i);
        val = json_object_dotget_value(conf_obj, param_name);
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) { /* Lora multi-SF channel disabled, nothing else to parse */
            printf("INFO: Lora multi-SF channel %i disabled\n", i);
        } else  { /* Lora multi-SF channel enabled, will parse the other parameters */
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.radio", i);
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, param_name);
            snprintf(param_name, sizeof param_name, "chan_multiSF_%i.if", i);
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, param_name);
            // TODO: handle individual SF enabling and disabling (spread_factor)
            printf("INFO: Lora multi-SF channel %i>  radio %i, IF %i Hz, 125 kHz bw, SF 5 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
        }
        /* all parameters parsed, submitting configuration to the HAL */
        if (lgw_rxif_setconf(i, &ifconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for Lora multi-SF channel %i\n", i);
            return -1;
        }
    }

    /* set configuration for Lora standard channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_Lora_std"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        printf("INFO: no configuration for Lora standard channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_Lora_std.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            printf("INFO: Lora standard channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.bandwidth");
            switch(bw) {
                case 500000: ifconf.bandwidth = BW_500KHZ; break;
                case 250000: ifconf.bandwidth = BW_250KHZ; break;
                case 125000: ifconf.bandwidth = BW_125KHZ; break;
                default: ifconf.bandwidth = BW_UNDEFINED;
            }
            sf = (uint32_t)json_object_dotget_number(conf_obj, "chan_Lora_std.spread_factor");
            switch(sf) {
                case  5: ifconf.datarate = DR_LORA_SF5;  break;
                case  6: ifconf.datarate = DR_LORA_SF6;  break;
                case  7: ifconf.datarate = DR_LORA_SF7;  break;
                case  8: ifconf.datarate = DR_LORA_SF8;  break;
                case  9: ifconf.datarate = DR_LORA_SF9;  break;
                case 10: ifconf.datarate = DR_LORA_SF10; break;
                case 11: ifconf.datarate = DR_LORA_SF11; break;
                case 12: ifconf.datarate = DR_LORA_SF12; break;
                default: ifconf.datarate = DR_UNDEFINED;
            }
            val = json_object_dotget_value(conf_obj, "chan_Lora_std.implicit_hdr");
            if (json_value_get_type(val) == JSONBoolean) {
                ifconf.implicit_hdr = (bool)json_value_get_boolean(val);
            } else {
                ifconf.implicit_hdr = false;
            }
            if (ifconf.implicit_hdr == true) {
                val = json_object_dotget_value(conf_obj, "chan_Lora_std.implicit_payload_length");
                if (json_value_get_type(val) == JSONNumber) {
                    ifconf.implicit_payload_length = (uint8_t)json_value_get_number(val);
                } else {
                    printf("ERROR: payload length setting is mandatory for implicit header mode\n");
                    return -1;
                }
                val = json_object_dotget_value(conf_obj, "chan_Lora_std.implicit_crc_en");
                if (json_value_get_type(val) == JSONBoolean) {
                    ifconf.implicit_crc_en = (bool)json_value_get_boolean(val);
                } else {
                    printf("ERROR: CRC enable setting is mandatory for implicit header mode\n");
                    return -1;
                }
                val = json_object_dotget_value(conf_obj, "chan_Lora_std.implicit_coderate");
                if (json_value_get_type(val) == JSONNumber) {
                    ifconf.implicit_coderate = (uint8_t)json_value_get_number(val);
                } else {
                    printf("ERROR: coding rate setting is mandatory for implicit header mode\n");
                    return -1;
                }
            }

            printf("INFO: Lora std channel> radio %i, IF %i Hz, %u Hz bw, SF %u, %s\n", ifconf.rf_chain, ifconf.freq_hz, bw, sf, (ifconf.implicit_hdr == true) ? "Implicit header" : "Explicit header");
        }
        if (lgw_rxif_setconf(8, &ifconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for Lora standard channel\n");
            return -1;
        }
    }

    /* set configuration for FSK channel */
    memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
    val = json_object_get_value(conf_obj, "chan_FSK"); /* fetch value (if possible) */
    if (json_value_get_type(val) != JSONObject) {
        printf("INFO: no configuration for FSK channel\n");
    } else {
        val = json_object_dotget_value(conf_obj, "chan_FSK.enable");
        if (json_value_get_type(val) == JSONBoolean) {
            ifconf.enable = (bool)json_value_get_boolean(val);
        } else {
            ifconf.enable = false;
        }
        if (ifconf.enable == false) {
            printf("INFO: FSK channel %i disabled\n", i);
        } else  {
            ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.radio");
            ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, "chan_FSK.if");
            bw = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.bandwidth");
            fdev = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.freq_deviation");
            ifconf.datarate = (uint32_t)json_object_dotget_number(conf_obj, "chan_FSK.datarate");

            /* if chan_FSK.bandwidth is set, it has priority over chan_FSK.freq_deviation */
            if ((bw == 0) && (fdev != 0)) {
                bw = 2 * fdev + ifconf.datarate;
            }
            if      (bw == 0)      ifconf.bandwidth = BW_UNDEFINED;
#if 0 /* TODO */
            else if (bw <= 7800)   ifconf.bandwidth = BW_7K8HZ;
            else if (bw <= 15600)  ifconf.bandwidth = BW_15K6HZ;
            else if (bw <= 31200)  ifconf.bandwidth = BW_31K2HZ;
            else if (bw <= 62500)  ifconf.bandwidth = BW_62K5HZ;
#endif
            else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
            else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
            else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
            else ifconf.bandwidth = BW_UNDEFINED;

            printf("INFO: FSK channel> radio %i, IF %i Hz, %u Hz bw, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
        }
        if (lgw_rxif_setconf(9, &ifconf) != LGW_HAL_SUCCESS) {
            printf("ERROR: invalid configuration for FSK channel\n");
            return -1;
        }
    }
    json_value_free(root_val);

    return 0;
}

static int parse_gateway_configuration(const char* conf_file, gw_s* gw) {
    const char conf_obj_name[] = "gateway_conf";
    JSON_Value *root_val;
    JSON_Object *conf_obj = NULL;
    JSON_Object *serv_obj = NULL;
    JSON_Array *serv_arry = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
    unsigned long long ull = 0;

    serv_s* serv_entry = NULL;

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        printf("ERROR: %s is not a valid JSON file\n", conf_file);
        return -1;
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        printf("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        return -1;
    } else {
        printf("INFO: %s does contain a JSON object named %s, parsing gateway parameters\n", conf_file, conf_obj_name);
    }

    /* gateway unique identifier (aka MAC address) (optional) */
    str = json_object_get_string(conf_obj, "gateway_ID");
    if (str != NULL) {
        sscanf(str, "%llx", &ull);
        gw->info.lgwm = ull;
        gw->info.net_mac_h = htonl((uint32_t)(0xFFFFFFFF & (ull>>32)));
        gw->info.net_mac_l = htonl((uint32_t)(0xFFFFFFFF &  ull  ));
        lgw_log(LOG_INFO, "INFO: gateway MAC address is configured to %016llX\n", ull);
    }

    str = json_object_get_string(conf_obj, "platform");
    if (str != NULL) {
        strncpy(gw->info.platform, str, sizeof gw->info.platform);
        gw->info.platform[sizeof gw->info.platform - 1] = '\0'; /* ensure string termination */
        lgw_log(LOG_INFO, "INFO: GPS serial port path is configured to \"%s\"\n", gw->info.platform);
    }

    str = json_object_get_string(conf_obj, "email");
    if (str != NULL) {
        strncpy(gw->info.email, str, sizeof gw->info.email);
        gw->info.email[sizeof gw->info.email - 1] = '\0'; /* ensure string termination */
        printf("INFO: GPS serial port path is configured to \"%s\"\n", gw->info.email);
    }

    str = json_object_get_string(conf_obj, "description");
    if (str != NULL) {
        strncpy(gw->info.description, str, sizeof gw->info.description);
        gw->info.description[sizeof gw->info.description - 1] = '\0'; /* ensure string termination */
        printf("INFO: GPS serial port path is configured to \"%s\"\n", gw->info.description);
    }

    /* GPS module TTY path (optional) */
    str = json_object_get_string(conf_obj, "gps_tty_path");
    if (str != NULL) {
        strncpy(gw->gps.gps_tty_path, str, sizeof gw->gps.gps_tty_path);
        gw->gps.gps_tty_path[sizeof gw->gps.gps_tty_path - 1] = '\0'; /* ensure string termination */
        printf("INFO: GPS serial port path is configured to \"%s\"\n", gw->gps.gps_tty_path);
    }

    /* get reference coordinates */
    val = json_object_get_value(conf_obj, "ref_latitude");
    if (val != NULL) {
        gw->gps.reference_coord.lat = (double)json_value_get_number(val);
        printf("INFO: Reference latitude is configured to %f deg\n", gw->gps.reference_coord.lat);
    }
    val = json_object_get_value(conf_obj, "ref_longitude");
    if (val != NULL) {
        gw->gps.reference_coord.lon = (double)json_value_get_number(val);
        printf("INFO: Reference longitude is configured to %f deg\n", gw->gps.reference_coord.lon);
    }
    val = json_object_get_value(conf_obj, "ref_altitude");
    if (val != NULL) {
        gw->gps.reference_coord.alt = (short)json_value_get_number(val);
        printf("INFO: Reference altitude is configured to %i meters\n", gw->gps.reference_coord.alt);
    }

    /* Gateway GPS coordinates hardcoding (aka. faking) option */
    val = json_object_get_value(conf_obj, "fake_gps");
    if (json_value_get_type(val) == JSONBoolean) {
        gw->gps.gps_fake_enable = (bool)json_value_get_boolean(val);
        if (gw->gps.gps_fake_enable == true) {
            printf("INFO: fake GPS is enabled\n");
        } else {
            printf("INFO: fake GPS is disabled\n");
        }
    }

    /* Beacon signal period (optional) */
    val = json_object_get_value(conf_obj, "beacon_period");
    if (val != NULL) {
        gw->beacon.beacon_period = (uint32_t)json_value_get_number(val);
        if ((gw->beacon.beacon_period > 0) && (gw->beacon.beacon_period < 6)) {
            printf("ERROR: invalid configuration for Beacon period, must be >= 6s\n");
            return -1;
        } else {
            printf("INFO: Beaconing period is configured to %u seconds\n", gw->beacon.beacon_period);
        }
    }

    /* Beacon TX frequency (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_hz");
    if (val != NULL) {
        gw->beacon.beacon_freq_hz = (uint32_t)json_value_get_number(val);
        printf("INFO: Beaconing signal will be emitted at %u Hz\n", gw->beacon.beacon_freq_hz);
    }

    /* Number of beacon channels (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_nb");
    if (val != NULL) {
        gw->beacon.beacon_freq_nb = (uint8_t)json_value_get_number(val);
        printf("INFO: Beaconing channel number is set to %u\n", gw->beacon.beacon_freq_nb);
    }

    /* Frequency step between beacon channels (optional) */
    val = json_object_get_value(conf_obj, "beacon_freq_step");
    if (val != NULL) {
        gw->beacon.beacon_freq_step = (uint32_t)json_value_get_number(val);
        printf("INFO: Beaconing channel frequency step is set to %uHz\n", gw->beacon.beacon_freq_step);
    }

    /* Beacon datarate (optional) */
    val = json_object_get_value(conf_obj, "beacon_datarate");
    if (val != NULL) {
        gw->beacon.beacon_datarate = (uint8_t)json_value_get_number(val);
        printf("INFO: Beaconing datarate is set to SF%d\n", gw->beacon.beacon_datarate);
    }

    /* Beacon modulation bandwidth (optional) */
    val = json_object_get_value(conf_obj, "beacon_bw_hz");
    if (val != NULL) {
        gw->beacon.beacon_bw_hz = (uint32_t)json_value_get_number(val);
        printf("INFO: Beaconing modulation bandwidth is set to %dHz\n", gw->beacon.beacon_bw_hz);
    }

    /* Beacon TX power (optional) */
    val = json_object_get_value(conf_obj, "beacon_power");
    if (val != NULL) {
        gw->beacon.beacon_power = (int8_t)json_value_get_number(val);
        printf("INFO: Beaconing TX power is set to %ddBm\n", gw->beacon.beacon_power);
    }

    /* Beacon information descriptor (optional) */
    val = json_object_get_value(conf_obj, "beacon_infodesc");
    if (val != NULL) {
        gw->beacon.beacon_infodesc = (uint8_t)json_value_get_number(val);
        printf("INFO: Beaconing information descriptor is set to %u\n", gw->beacon.beacon_infodesc);
    }

    /* Auto-quit threshold (optional) */
    val = json_object_get_value(conf_obj, "autoquit_threshold");
    if (val != NULL) {
        gw->cfg.autoquit_threshold = (uint32_t)json_value_get_number(val);
        printf("INFO: Auto-quit after %u non-acknowledged PULL_DATA\n", gw->cfg.autoquit_threshold);
    }

    /* servers configure */
	serv_arry = json_object_get_array(conf_obj, "servers");
	if (serv_arry != NULL) {
		/* serv_count represents the maximal number of servers to be read. */
        int count = 0, i = 0, try = 0;
		count = json_array_get_count(serv_arry);
		printf("INFO: Found %i servers in array.\n", count);
		for (i = 0; i < count; i++) {
            serv_entry = (serv_s*)lgw_malloc(sizeof(serv_entry));
            if (NULL == serv_entry) {
                printf("WARNING~ Can't allocates for server, ignored\n");
                if (try++ < 3) /* total try 3 times */ 
                    --i;             
                continue;
            }

            /* 在这里初始化service */
            serv_entry->list.next = NULL;
            serv_entry->rxpkt_serv = NULL;

            /* service network information */
            serv_entry->net = (serv_net_s*)lgw_malloc(sizeof(serv_net_s));
            if (NULL == serv_entry->net) {
                printf("WARNING~ Can't allocates for server net struct, ignored\n");
                if (try++ < 3) /* total try 3 times */ 
                    --i;             
                lgw_free(serv_entry);
                continue;
            }
            serv_entry->net->sock_up = -1;
            serv_entry->net->sock_down = -1;
            serv_entry->net->push_timeout_half.tv_sec = 0;
            serv_entry->net->push_timeout_half.tv_usec = DEFAULT_PUSH_TIMEOUT_MS * 500;
            serv_entry->net->pull_timeout.tv_sec = 0;
            serv_entry->net->pull_timeout.tv_usec = DEFAULT_PULL_TIMEOUT_MS * 1000;
            serv_entry->net->pull_interval = DEFAULT_PULL_INTERVAL;
            
            /* about service report information */
            serv_entry->report->report_ready = false;
            serv_entry->report->stat_interval = DEFAULT_STAT_INTERVAL;

            /* about service filter information */
            serv_entry->filter.fwd_valid_pkt = true;
            serv_entry->filter.fwd_error_pkt = false;
            serv_entry->filter.fwd_nocrc_pkt = false;
            serv_entry->filter.fport = 0;
            serv_entry->filter.devaddr = 0;

            serv_entry->report = NULL;

            /* about service status information */
            serv_entry->state.live = false;
            serv_entry->state.contact = 0;

            try = 0;

            do {
                if (sem_init(&serv_entry->thread.sema, 0, 0) != 0) {
                    try++;
                } else
                    break;
            } while (try < 3);

            if (try == 3) { /* 等于3时，sem的初始化已经失败了3次 */
                printf("WARNING, Can't initializes the unnamed semaphore of service, ignore this element.\n");
                lgw_free(serv_entry);
                continue;
            }

            serv_entry->thread.stop_sig = false;

			serv_obj = json_array_get_object(serv_arry, i);

			str = json_object_get_string(serv_obj, "server_name");
            if (str != NULL) {
                strncpy(serv_entry->info.name, str, sizeof serv_entry->info.name);
                serv_entry->info.name[sizeof serv_entry->info.name - 1] = '\0'; /* ensure string termination */
                printf("INFO: Found a server name is \"%s\"\n", str);
            } else {
                lgw_gen_str((char*)&serv_entry->info.name, sizeof(serv_entry->info.name));
                printf("INFO: The server name mustbe configure, generate a random name: %s\n", serv_entry->info.name);
                continue;
            }

			str = json_object_get_string(serv_obj, "server_type");
            if (str != NULL) {
				if (!strncmp(str, "semtech", 7)) {
					serv_entry->info.type = semtech;
                    /* 如果是semtech类型，则需要初始化report相关的数据 */
                    serv_entry->report = (report_s*)lgw_malloc(sizeof(report_s));
                    pthread_mutex_init(&serv_entry->report->mx_report, NULL);
				} else if (!strncmp(str, "ttn", 3)) {
					serv_entry->info.type = ttn;
				} else if (!strncmp(str, "mqtt", 4)) {
					serv_entry->info.type = mqtt;
				} else if (!strncmp(str, "gwtraf", 6)) {
					serv_entry->info.type = gwtraf;
				} else 
					serv_entry->info.type = semtech;
            } else {
					serv_entry->info.type = semtech;  // 默认的服务是semtech
            }
            
			val = json_object_get_value(serv_obj, "enabled");
            if (json_value_get_type(val) == JSONBoolean) 
                serv_entry->info.enabled = json_value_get_boolean(val);
            else 
                serv_entry->info.enabled = true;   // 默认是开启的

			str = json_object_get_string(serv_obj, "server_address");
            if (str != NULL) {
                strncpy(serv_entry->net->addr, str, sizeof serv_entry->net->addr);
                serv_entry->net->addr[sizeof serv_entry->net->addr - 1] = '\0'; /* ensure string termination */
                printf("INFO: Found a server name is \"%s\"\n", str);
            } else {  //如果没有设置服务地址，就释放内存，读入下一条记录
                lgw_free(serv_entry);
                continue;
            }
			str = json_object_get_string(serv_obj, "serv_port_up");
            if (str != NULL) {
                strncpy(serv_entry->net->port_up, str, sizeof serv_entry->net->port_up);
                serv_entry->net->port_up[sizeof serv_entry->net->port_up - 1] = '\0'; /* ensure string termination */
                printf("INFO: Found a serv_port_up is \"%s\"\n", serv_entry->net->port_up);
            }
			str = json_object_get_string(serv_obj, "serv_port_down");
            if (str != NULL) {
                strncpy(serv_entry->net->port_down, str, sizeof serv_entry->net->port_down);
                serv_entry->net->port_down[sizeof serv_entry->net->port_down - 1] = '\0'; /* ensure string termination */
                printf("INFO: Found a serv_port_down \"%s\"\n", serv_entry->net->port_down);
            }

            val = json_object_get_value(serv_obj, "push_timeout_ms");
            if (val != NULL) {
                serv_entry->net->push_timeout_half.tv_usec = 500 * (long int)json_value_get_number(val);
            }

            val = json_object_get_value(serv_obj, "pull_timeout_ms");
            if (val != NULL) {
                serv_entry->net->pull_timeout.tv_usec = 1000 * (long int)json_value_get_number(val);
            }

            val = json_object_get_value(serv_obj, "pull_interval");
            if (val != NULL) {
                serv_entry->net->pull_interval = (int)json_value_get_number(val);
            }

            val = json_object_get_value(serv_obj, "stat_interval");
            if (val != NULL) {
                serv_entry->report->stat_interval = (int)json_value_get_number(val);
                printf("INFO: Found a stat_interval is \"%d\"\n", serv_entry->report->stat_interval);
            }

            val = json_object_get_value(serv_obj, "forward_crc_valid");
            if (json_value_get_type(val) == JSONBoolean) {
                serv_entry->filter.fwd_valid_pkt = (bool)json_value_get_boolean(val);
            }
            printf("INFO: packets received with a valid CRC will%s be forwarded\n", (serv_entry->filter.fwd_valid_pkt ? "" : " NOT"));
            val = json_object_get_value(serv_obj, "forward_crc_error");
            if (json_value_get_type(val) == JSONBoolean) {
                serv_entry->filter.fwd_error_pkt = (bool)json_value_get_boolean(val);
            }
            printf("INFO: packets received with a CRC error will%s be forwarded\n", (serv_entry->filter.fwd_error_pkt ? "" : " NOT"));
            val = json_object_get_value(serv_obj, "forward_crc_disabled");
            if (json_value_get_type(val) == JSONBoolean) {
                serv_entry->filter.fwd_nocrc_pkt = (bool)json_value_get_boolean(val);
            }
            printf("INFO: packets received with a no CRC will%s be forwarded\n", (serv_entry->filter.fwd_nocrc_pkt ? "" : " NOT"));
            val = json_object_get_value(serv_obj, "filter_fport");
            if (val != NULL) {
                serv_entry->filter.fport = (uint8_t)json_value_get_number(val);
            } 

            val = json_object_get_value(serv_obj, "filter_devaddr");
            if (val != NULL) {
                serv_entry->filter.devaddr = (uint8_t)json_value_get_number(val);
            } 

            LGW_LIST_INSERT_TAIL(&gw->serv_list, serv_entry, list);
        }
    } else 
        printf("WARNING: No service offer.\n");

    /* free JSON parsing data structure */
    json_value_free(root_val);
    return 0;
}

static int parse_debug_configuration(const char * conf_file) {
    int i;
    const char conf_obj_name[] = "debug_conf";
    JSON_Value *root_val;
    JSON_Object *conf_obj = NULL;
    JSON_Array *conf_array = NULL;
    JSON_Object *conf_obj_array = NULL;
    const char *str; /* pointer to sub-strings in the JSON data */

    struct lgw_conf_debug_s debugconf;

    /* Initialize structure */
    memset(&debugconf, 0, sizeof debugconf);

    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        printf("ERROR: %s is not a valid JSON file\n", conf_file);
        return -1;
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
    if (conf_obj == NULL) {
        printf("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
        json_value_free(root_val);
        return -1;
    } else {
        printf("INFO: %s does contain a JSON object named %s, parsing debug parameters\n", conf_file, conf_obj_name);
    }

    /* Get reference payload configuration */
    conf_array = json_object_get_array (conf_obj, "ref_payload");
    if (conf_array != NULL) {
        debugconf.nb_ref_payload = json_array_get_count(conf_array);
        printf("INFO: got %u debug reference payload\n", debugconf.nb_ref_payload);

        for (i = 0; i < (int)debugconf.nb_ref_payload; i++) {
            conf_obj_array = json_array_get_object(conf_array, i);
            /* id */
            str = json_object_get_string(conf_obj_array, "id");
            if (str != NULL) {
                sscanf(str, "0x%08X", &(debugconf.ref_payload[i].id));
                printf("INFO: reference payload ID %d is 0x%08X\n", i, debugconf.ref_payload[i].id);
            }

            /* global count */
            GW.log.nb_pkt_received_ref[i] = 0;
        }
    }

    /* Get log file configuration */
    str = json_object_get_string(conf_obj, "log_file");
    if (str != NULL) {
        strncpy(debugconf.log_file_name, str, sizeof debugconf.log_file_name);
        debugconf.log_file_name[sizeof debugconf.log_file_name - 1] = '\0'; /* ensure string termination */
        printf("INFO: setting debug log file name to %s\n", debugconf.log_file_name);
    }

    /* Commit configuration */
    if (lgw_debug_setconf(&debugconf) != LGW_HAL_SUCCESS) {
        printf("ERROR: Failed to configure debug\n");
        json_value_free(root_val);
        return -1;
    }

    /* free JSON parsing data structure */
    json_value_free(root_val);
    return 0;
}

int parsecfg(const char *cfgfile, gw_s* gw) {
    int ret;
    if (!strncmp(gw->hal.board, "LG301", 5)) {
        ret = parse_SX130x_configuration(cfgfile, gw);
    } else if (!strncmp(gw->hal.board, "LG302", 5)) { 
        ret = parse_SX130x_configuration(cfgfile, gw);
    } else if (!strncmp(gw->hal.board, "LG308", 5)) { 
        ret = parse_SX130x_configuration(cfgfile, gw);
    //} else if (!strncmp(gw->hal.board, "LG02", 4)) { 
    //    ret = parse_lg02_configuration(cfgfile, gw);
    //} else if (!strncmp(gw->hal.board, "LG01", 4)) { 
    //    ret = parse_lg02_configuration(cfgfile, gw);
    } else 
        ret = parse_SX130x_configuration(cfgfile, gw);
    ret |= parse_gateway_configuration(cfgfile, gw);
    return (ret);
}
