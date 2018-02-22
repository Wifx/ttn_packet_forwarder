/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
	Configure Lora concentrator and forward packets to multiple servers
	Use GPS for packet timestamping.
	Send a becon at a regular interval without server intervention
	Processes ghost packets
	Switchable tasks.
	Suited for compilation on OSX

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Ruud Vlaming
*/
#include "utils.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loragw_hal.h"
#include "parson.h"
#include "monitor.h"

/* Log system */
static char *log_output = NULL;                         /* log file path if any */

void log_set_output(char *log_out){
	log_output = log_out;
}

int log_msg(const char *format, ...){         /* message that is destined to the user */
    va_list arg;
    int done;

    va_start(arg, format);

    /* print text to stdout */
    done = vprintf(format, arg);

    if(log_output != NULL){
        FILE *file = fopen(log_output, "a+");
        if(file != NULL){
            /* if log file option and success to open the file */
            done = vfprintf(file, format, arg);
            fclose(file);
        }
    }

    va_end(arg);

    return done;
}

int parse_gateway_configuration(const char * conf_file, struct gateway_conf *gtw_conf) {
	const char conf_obj_name[] = "gateway_conf";
	JSON_Value *root_val;
	JSON_Object *conf_obj = NULL;
	JSON_Value *val = NULL; /* needed to detect the absence of some fields */
	JSON_Value *val1 = NULL; /* needed to detect the absence of some fields */
	JSON_Value *val2 = NULL; /* needed to detect the absence of some fields */
	JSON_Array *servers = NULL;
	JSON_Array *syscalls = NULL;
	const char *str; /* pointer to sub-strings in the JSON data */
	unsigned long long ull = 0;
	int i; /* Loop variable */
	int ic; /* Server counter */

	/* try to parse JSON */
	root_val = json_parse_file_with_comments(conf_file);
	if (root_val == NULL) {
		log_msg("ERROR: %s is not a valid JSON file\n", conf_file);
		exit(EXIT_FAILURE);
	}

	/* point to the gateway configuration object */
	conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
	if (conf_obj == NULL) {
		log_msg("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
		return -1;
	} else {
		log_msg("INFO: %s does contain a JSON object named %s, parsing gateway parameters\n", conf_file, conf_obj_name);
	}

	/* gateway unique identifier (aka MAC address) (optional) */
	str = json_object_get_string(conf_obj, "gateway_ID");
	if (str != NULL) {
		sscanf(str, "%llx", &ull);
		gtw_conf->lgwm = ull;
		log_msg("INFO: gateway MAC address is configured to %016llX\n", ull);
	}

	/* Obtain multiple servers hostnames and ports from array */
	JSON_Object *nw_server = NULL;
	servers = json_object_get_array(conf_obj, "servers");
	if (servers != NULL) {
		/* gtw_conf->serv_count represents the maximal number of servers to be read. */
		gtw_conf->serv_count = json_array_get_count(servers);
		log_msg("INFO: Found %i servers in array.\n", gtw_conf->serv_count);
		ic = 0;
		for (i = 0; i < gtw_conf->serv_count  && ic < MAX_SERVERS; i++) {
			gtw_conf->serv_enable[i] = true;
			nw_server = json_array_get_object(servers,i);
			str = json_object_get_string(nw_server, "server_address");
			val = json_object_get_value(nw_server, "serv_enabled");
			val1 = json_object_get_value(nw_server, "serv_port_up");
			val2 = json_object_get_value(nw_server, "serv_port_down");
			/* Try to read the fields */
			if (str != NULL)  strncpy(gtw_conf->serv_addr[ic], str, sizeof gtw_conf->serv_addr[ic]);
			if (val1 != NULL) snprintf(gtw_conf->serv_port_up[ic], sizeof gtw_conf->serv_port_up[ic], "%u", (uint16_t)json_value_get_number(val1));
			if (val2 != NULL) snprintf(gtw_conf->serv_port_down[ic], sizeof gtw_conf->serv_port_down[ic], "%u", (uint16_t)json_value_get_number(val2));
			/* If there is no server name we can only silently progress to the next entry */
			if (str == NULL) {
				continue;
			}
			/* If there are no ports report and progress to the next entry */
			else if ((val1 == NULL) || (val2 == NULL)) {
				log_msg("INFO: Skipping server \"%s\" with at least one invalid port number\n", gtw_conf->serv_addr[ic]);
				continue;
			}
            /* If the server was explicitly disabled, report and progress to the next entry */
			else if ( (val != NULL) && ((json_value_get_type(val)) == JSONBoolean) && ((bool)json_value_get_boolean(val) == false )) {
				log_msg("INFO: Skipping disabled server \"%s\"\n", gtw_conf->serv_addr[ic]);
				continue;
			}
			/* All test survived, this is a valid server, report and increase server counter. */
			log_msg("INFO: Server %i configured to \"%s\", with port up \"%s\" and port down \"%s\"\n", ic, gtw_conf->serv_addr[ic],gtw_conf->serv_port_up[ic],gtw_conf->serv_port_down[ic]);
			/* The server may be valid, it is not yet live. */
			gtw_conf->serv_live[ic] = false;
			ic++;
		}
		gtw_conf->serv_count = ic;
	} else {
		/* If there are no servers in server array fall back to old fashioned single server definition.
		 * The difference with the original situation is that we require a complete definition. */
		/* server hostname or IP address (optional) */
		str = json_object_get_string(conf_obj, "server_address");
		val1 = json_object_get_value(conf_obj, "serv_port_up");
		val2 = json_object_get_value(conf_obj, "serv_port_down");
		if ((str != NULL) && (val1 != NULL) && (val2 != NULL)) {
			gtw_conf->serv_count = 1;
			gtw_conf->serv_live[0] = false;
			strncpy(gtw_conf->serv_addr[0], str, sizeof gtw_conf->serv_addr[0]);
			snprintf(gtw_conf->serv_port_up[0], sizeof gtw_conf->serv_port_up[0], "%u", (uint16_t)json_value_get_number(val1));
			snprintf(gtw_conf->serv_port_down[0], sizeof gtw_conf->serv_port_down[0], "%u", (uint16_t)json_value_get_number(val2));
			log_msg("INFO: Server configured to \"%s\", with port up \"%s\" and port down \"%s\"\n", gtw_conf->serv_addr[0],gtw_conf->serv_port_up[0],gtw_conf->serv_port_down[0]);
		}
	}


	/* Using the defaults in case no values are present in the JSON */
	//TODO: Eliminate this default behavior, the server should be well configured or stop.
	if (gtw_conf->serv_count == 0) {
		log_msg("INFO: Using defaults for server and ports (specific ports are ignored if no server is defined)");
		strncpy(gtw_conf->serv_addr[0],STR(DEFAULT_SERVER),sizeof(STR(DEFAULT_SERVER)));
		strncpy(gtw_conf->serv_port_up[0],STR(DEFAULT_PORT_UP),sizeof(STR(DEFAULT_PORT_UP)));
		strncpy(gtw_conf->serv_port_down[0],STR(DEFAULT_PORT_DW),sizeof(STR(DEFAULT_PORT_DW)));
		gtw_conf->serv_live[0] = false;
		gtw_conf->serv_count = 1;
	}

	/* Read the system calls for the monitor function. */
	syscalls = json_object_get_array(conf_obj, "system_calls");
	if (syscalls != NULL) {
		/* gtw_conf->serv_count represents the maximal number of servers to be read. */
		mntr_sys_count = json_array_get_count(syscalls);
		log_msg("INFO: Found %i system calls in array.\n", mntr_sys_count);
		for (i = 0; i < mntr_sys_count  && i < MNTR_SYS_MAX; i++) {
			str = json_array_get_string(syscalls,i);
			strncpy(mntr_sys_list[i], str, sizeof mntr_sys_list[i]);
			log_msg("INFO: System command %i: \"%s\"\n",i,mntr_sys_list[i]);
		}
	}

	/* monitor hostname or IP address (optional) */
	str = json_object_get_string(conf_obj, "monitor_address");
	if (str != NULL) {
		strncpy(gtw_conf->monitor_addr, str, sizeof gtw_conf->monitor_addr);
		log_msg("INFO: monitor hostname or IP address is configured to \"%s\"\n", gtw_conf->monitor_addr);
	}

	/* get monitor connection port (optional) */
	val = json_object_get_value(conf_obj, "monitor_port");
	if (val != NULL) {
		snprintf(gtw_conf->monitor_port, sizeof gtw_conf->monitor_port, "%u", (uint16_t)json_value_get_number(val));
		log_msg("INFO: monitor port is configured to \"%s\"\n", gtw_conf->monitor_port);
	}

	/* ghost hostname or IP address (optional) */
	str = json_object_get_string(conf_obj, "ghost_address");
	if (str != NULL) {
		strncpy(gtw_conf->ghost_addr, str, sizeof gtw_conf->ghost_addr);
		log_msg("INFO: ghost hostname or IP address is configured to \"%s\"\n", gtw_conf->ghost_addr);
	}

	/* get ghost connection port (optional) */
	val = json_object_get_value(conf_obj, "ghost_port");
	if (val != NULL) {
		snprintf(gtw_conf->ghost_port, sizeof gtw_conf->ghost_port, "%u", (uint16_t)json_value_get_number(val));
		log_msg("INFO: ghost port is configured to \"%s\"\n", gtw_conf->ghost_port);
	}

	/* get keep-alive interval (in seconds) for downstream (optional) */
	val = json_object_get_value(conf_obj, "keepalive_interval");
	if (val != NULL) {
		gtw_conf->keepalive_time = (int)json_value_get_number(val);
		log_msg("INFO: downstream keep-alive interval is configured to %i seconds\n", gtw_conf->keepalive_time);
	}

	/* get interval (in seconds) for statistics display (optional) */
	val = json_object_get_value(conf_obj, "stat_interval");
	if (val != NULL) {
		gtw_conf->stat_interval = (unsigned)json_value_get_number(val);
		log_msg("INFO: statistics display interval is configured to %i seconds\n", gtw_conf->stat_interval);
	}

	/* get time-out value (in ms) for upstream datagrams (optional) */
	val = json_object_get_value(conf_obj, "push_timeout_ms");
	if (val != NULL) {
		gtw_conf->push_timeout_half.tv_usec = 500 * (long int)json_value_get_number(val);
		log_msg("INFO: upstream PUSH_DATA time-out is configured to %u ms\n", (unsigned)(gtw_conf->push_timeout_half.tv_usec / 500));
	}

	/* packet filtering parameters */
	val = json_object_get_value(conf_obj, "forward_crc_valid");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->fwd_valid_pkt = (bool)json_value_get_boolean(val);
	}
	log_msg("INFO: packets received with a valid CRC will%s be forwarded\n", (gtw_conf->fwd_valid_pkt ? "" : " NOT"));
	val = json_object_get_value(conf_obj, "forward_crc_error");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->fwd_error_pkt = (bool)json_value_get_boolean(val);
	}
	log_msg("INFO: packets received with a CRC error will%s be forwarded\n", (gtw_conf->fwd_error_pkt ? "" : " NOT"));
	val = json_object_get_value(conf_obj, "forward_crc_disabled");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->fwd_nocrc_pkt = (bool)json_value_get_boolean(val);
	}
	log_msg("INFO: packets received with no CRC will%s be forwarded\n", (gtw_conf->fwd_nocrc_pkt ? "" : " NOT"));

	/* GPS module TTY path (optional) */
	str = json_object_get_string(conf_obj, "gps_tty_path");
	if (str != NULL) {
		strncpy(gtw_conf->gps_tty_path, str, sizeof gtw_conf->gps_tty_path);
		log_msg("INFO: GPS serial port path is configured to \"%s\"\n", gtw_conf->gps_tty_path);
	}

	/* SSH path (optional) */
	str = json_object_get_string(conf_obj, "ssh_path");
	if (str != NULL) {
		strncpy(ssh_path, str, sizeof ssh_path);
		log_msg("INFO: SSH path is configured to \"%s\"\n", ssh_path);
	}

	/* SSH port (optional) */
	val = json_object_get_value(conf_obj, "ssh_port");
	if (val != NULL) {
		ssh_port = (uint16_t) json_value_get_number(val);
		log_msg("INFO: SSH port is configured to %u\n", ssh_port);
	}

	/* WEB port (optional) */
	val = json_object_get_value(conf_obj, "http_port");
	if (val != NULL) {
		http_port = (uint16_t) json_value_get_number(val);
		log_msg("INFO: HTTP port is configured to %u\n", http_port);
	}

	/* NGROK path (optional) */
	str = json_object_get_string(conf_obj, "ngrok_path");
	if (str != NULL) {
		strncpy(ngrok_path, str, sizeof ngrok_path);
		log_msg("INFO: NGROK path is configured to \"%s\"\n", ngrok_path);
	}

	/* get reference coordinates */
	val = json_object_get_value(conf_obj, "ref_latitude");
	if (val != NULL) {
		gtw_conf->reference_coord.lat = (double)json_value_get_number(val);
		log_msg("INFO: Reference latitude is configured to %f deg\n", gtw_conf->reference_coord.lat);
	}
	val = json_object_get_value(conf_obj, "ref_longitude");
	if (val != NULL) {
		gtw_conf->reference_coord.lon = (double)json_value_get_number(val);
		log_msg("INFO: Reference longitude is configured to %f deg\n", gtw_conf->reference_coord.lon);
	}
	val = json_object_get_value(conf_obj, "ref_altitude");
	if (val != NULL) {
		gtw_conf->reference_coord.alt = (short)json_value_get_number(val);
		log_msg("INFO: Reference altitude is configured to %i meters\n", gtw_conf->reference_coord.alt);
	}

	/* Read the value for gtw_conf->gps_enabled data */
	val = json_object_get_value(conf_obj, "gps");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->gps_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->gps_enabled == true) {
		log_msg("INFO: GPS is enabled\n");
	} else {
		log_msg("INFO: GPS is disabled\n");
    }

	if (gtw_conf->gps_enabled == true) {
		/* Gateway GPS coordinates hardcoding (aka. faking) option */
		val = json_object_get_value(conf_obj, "fake_gps");
		if (json_value_get_type(val) == JSONBoolean) {
			gtw_conf->gps_fake_enable = (bool)json_value_get_boolean(val);
			if (gtw_conf->gps_fake_enable == true) {
				log_msg("INFO: Using fake GPS coordinates instead of real.\n");
			} else {
				log_msg("INFO: Using real GPS if available.\n");
			}
		}
	}

	/* Beacon signal period (optional) */
	val = json_object_get_value(conf_obj, "beacon_period");
	if (val != NULL) {
		gtw_conf->beacon_period = (uint32_t)json_value_get_number(val);
		log_msg("INFO: Beaconing period is configured to %u seconds\n", gtw_conf->beacon_period);
	}

	/* Beacon signal period (optional) */
	val = json_object_get_value(conf_obj, "beacon_offset");
	if (val != NULL) {
		gtw_conf->beacon_offset = (uint32_t)json_value_get_number(val);
		log_msg("INFO: Beaconing signal offset is configured to %u seconds\n", gtw_conf->beacon_offset);
	}

	/* Beacon TX frequency (optional) */
	val = json_object_get_value(conf_obj, "beacon_freq_hz");
	if (val != NULL) {
		gtw_conf->beacon_freq_hz = (uint32_t)json_value_get_number(val);
		log_msg("INFO: Beaconing signal will be emitted at %u Hz\n", gtw_conf->beacon_freq_hz);
	}

	/* Read the value for upstream data */
	val = json_object_get_value(conf_obj, "upstream");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->upstream_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->upstream_enabled == true) {
		log_msg("INFO: Upstream data is enabled\n");
	} else {
		log_msg("INFO: Upstream data is disabled\n");
	}

	/* Read the value for gtw_conf->downstream_enabled data */
	val = json_object_get_value(conf_obj, "downstream");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->downstream_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->downstream_enabled == true) {
		log_msg("INFO: Downstream data is enabled\n");
	} else {
		log_msg("INFO: Downstream data is disabled\n");
	}

	/* Read the value for ghoststream_enabled data */
	val = json_object_get_value(conf_obj, "ghoststream");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->ghoststream_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->ghoststream_enabled == true) {
		log_msg("INFO: Ghoststream data is enabled\n");
	} else {
		log_msg("INFO: Ghoststream data is disabled\n");
	}

	/* Read the value for radiostream_enabled data */
	val = json_object_get_value(conf_obj, "radiostream");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->radiostream_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->radiostream_enabled == true) {
		log_msg("INFO: Radiostream data is enabled\n");
	} else {
		log_msg("INFO: Radiostream data is disabled\n");
    }

	/* Read the value for statusstream_enabled data */
	val = json_object_get_value(conf_obj, "statusstream");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->statusstream_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->statusstream_enabled == true) {
		log_msg("INFO: Statusstream data is enabled\n");
	} else {
		log_msg("INFO: Statusstream data is disabled\n");
    }

	/* Read the value for gtw_conf->beacon_enabled data */
	val = json_object_get_value(conf_obj, "beacon");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->beacon_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->beacon_enabled == true) {
		log_msg("INFO: Beacon is enabled\n");
	} else {
		log_msg("INFO: Beacon is disabled\n");
    }

	/* Read the value for gtw_conf->monitor_enabled data */
	val = json_object_get_value(conf_obj, "monitor");
	if (json_value_get_type(val) == JSONBoolean) {
		gtw_conf->monitor_enabled = (bool)json_value_get_boolean(val);
	}
	if (gtw_conf->monitor_enabled == true) {
		log_msg("INFO: Monitor is enabled\n");
	} else {
		log_msg("INFO: Monitor is disabled\n");
    }

	/* Auto-quit threshold (optional) */
	val = json_object_get_value(conf_obj, "autoquit_threshold");
	if (val != NULL) {
		gtw_conf->autoquit_threshold = (uint32_t)json_value_get_number(val);
		log_msg("INFO: Auto-quit after %u non-acknowledged PULL_DATA\n", gtw_conf->autoquit_threshold);
	}

	/* Platform read and override */
	str = json_object_get_string(conf_obj, "platform");
	if (str != NULL) {
		if (strncmp(str, "*", 1) != 0) { strncpy(gtw_conf->platform, str, sizeof gtw_conf->platform); }
		log_msg("INFO: Platform configured to \"%s\"\n", gtw_conf->platform);
	}

	/* Read of contact gtw_conf->email */
	str = json_object_get_string(conf_obj, "contact_email");
	if (str != NULL) {
		strncpy(gtw_conf->email, str, sizeof gtw_conf->email);
		log_msg("INFO: Contact gtw_conf->email configured to \"%s\"\n", gtw_conf->email);
	}

	/* Read of gtw_conf->description */
	str = json_object_get_string(conf_obj, "description");
	if (str != NULL) {
		strncpy(gtw_conf->description, str, sizeof gtw_conf->description);
		log_msg("INFO: Description configured to \"%s\"\n", gtw_conf->description);
	}

	/* free JSON parsing data structure */
	json_value_free(root_val);
	return 0;
}

int parse_SX1301_configuration(const char * conf_file) {
	int i;
	char param_name[32]; /* used to generate variable parameter names */
	const char *str; /* used to store string value from JSON object */
	const char conf_obj_name[] = "SX1301_conf";
	JSON_Value *root_val = NULL;
	JSON_Object *conf_obj = NULL;
	JSON_Value *val = NULL;
	struct lgw_conf_board_s boardconf;
	struct lgw_conf_rxrf_s rfconf;
	struct lgw_conf_rxif_s ifconf;
	uint32_t sf, bw, fdev;
	struct lgw_tx_gain_lut_s txlut;

	/* try to parse JSON */
	root_val = json_parse_file_with_comments(conf_file);
	if (root_val == NULL) {
		log_msg("ERROR: %s is not a valid JSON file\n", conf_file);
		exit(EXIT_FAILURE);
	}

	/* point to the gateway configuration object */
	conf_obj = json_object_get_object(json_value_get_object(root_val), conf_obj_name);
	if (conf_obj == NULL) {
		log_msg("INFO: %s does not contain a JSON object named %s\n", conf_file, conf_obj_name);
		return -1;
	} else {
		log_msg("INFO: %s does contain a JSON object named %s, parsing SX1301 parameters\n", conf_file, conf_obj_name);
	}

	/* set board configuration */
	memset(&boardconf, 0, sizeof boardconf); /* initialize configuration structure */
	val = json_object_get_value(conf_obj, "lorawan_public"); /* fetch value (if possible) */
	if (json_value_get_type(val) == JSONBoolean) {
		boardconf.lorawan_public = (bool)json_value_get_boolean(val);
	} else {
		log_msg("WARNING: Data type for lorawan_public seems wrong, please check\n");
		boardconf.lorawan_public = false;
	}
	val = json_object_get_value(conf_obj, "clksrc"); /* fetch value (if possible) */
	if (json_value_get_type(val) == JSONNumber) {
		boardconf.clksrc = (uint8_t)json_value_get_number(val);
	} else {
		log_msg("WARNING: Data type for clksrc seems wrong, please check\n");
		boardconf.clksrc = 0;
	}
	log_msg("INFO: lorawan_public %d, clksrc %d\n", boardconf.lorawan_public, boardconf.clksrc);
	/* all parameters parsed, submitting configuration to the HAL */
        if (lgw_board_setconf(boardconf) != LGW_HAL_SUCCESS) {
                log_msg("WARNING: Failed to configure board\n");
	}

	/* set configuration for tx gains */
	memset(&txlut, 0, sizeof txlut); /* initialize configuration structure */
	for (i = 0; i < TX_GAIN_LUT_SIZE_MAX; i++) {
		snprintf(param_name, sizeof param_name, "tx_lut_%i", i); /* compose parameter path inside JSON structure */
		val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
		if (json_value_get_type(val) != JSONObject) {
			log_msg("INFO: no configuration for tx gain lut %i\n", i);
			continue;
		}
		txlut.size++; /* update TX LUT size based on JSON object found in configuration file */
		/* there is an object to configure that TX gain index, let's parse it */
		snprintf(param_name, sizeof param_name, "tx_lut_%i.pa_gain", i);
		val = json_object_dotget_value(conf_obj, param_name);
		if (json_value_get_type(val) == JSONNumber) {
			txlut.lut[i].pa_gain = (uint8_t)json_value_get_number(val);
		} else {
			log_msg("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
			txlut.lut[i].pa_gain = 0;
		}
                snprintf(param_name, sizeof param_name, "tx_lut_%i.dac_gain", i);
                val = json_object_dotget_value(conf_obj, param_name);
                if (json_value_get_type(val) == JSONNumber) {
                        txlut.lut[i].dac_gain = (uint8_t)json_value_get_number(val);
                } else {
                        txlut.lut[i].dac_gain = 3; /* This is the only dac_gain supported for now */
                }
                snprintf(param_name, sizeof param_name, "tx_lut_%i.dig_gain", i);
                val = json_object_dotget_value(conf_obj, param_name);
                if (json_value_get_type(val) == JSONNumber) {
                        txlut.lut[i].dig_gain = (uint8_t)json_value_get_number(val);
                } else {
			log_msg("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
                        txlut.lut[i].dig_gain = 0;
                }
                snprintf(param_name, sizeof param_name, "tx_lut_%i.mix_gain", i);
                val = json_object_dotget_value(conf_obj, param_name);
                if (json_value_get_type(val) == JSONNumber) {
                        txlut.lut[i].mix_gain = (uint8_t)json_value_get_number(val);
                } else {
			log_msg("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
                        txlut.lut[i].mix_gain = 0;
                }
                snprintf(param_name, sizeof param_name, "tx_lut_%i.rf_power", i);
                val = json_object_dotget_value(conf_obj, param_name);
                if (json_value_get_type(val) == JSONNumber) {
                        txlut.lut[i].rf_power = (int8_t)json_value_get_number(val);
                } else {
			log_msg("WARNING: Data type for %s[%d] seems wrong, please check\n", param_name, i);
                        txlut.lut[i].rf_power = 0;
                }
	}
	/* all parameters parsed, submitting configuration to the HAL */
	log_msg("INFO: Configuring TX LUT with %u indexes\n", txlut.size);
        if (lgw_txgain_setconf(&txlut) != LGW_HAL_SUCCESS) {
                log_msg("WARNING: Failed to configure concentrator TX Gain LUT\n");
	}

	/* set configuration for RF chains */
	for (i = 0; i < LGW_RF_CHAIN_NB; ++i) {
		memset(&rfconf, 0, sizeof rfconf); /* initialize configuration structure */
		snprintf(param_name, sizeof param_name, "radio_%i", i); /* compose parameter path inside JSON structure */
		val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
		if (json_value_get_type(val) != JSONObject) {
			log_msg("INFO: no configuration for radio %i\n", i);
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
			log_msg("INFO: radio %i disabled\n", i);
		} else  { /* radio enabled, will parse the other parameters */
			snprintf(param_name, sizeof param_name, "radio_%i.freq", i);
			rfconf.freq_hz = (uint32_t)json_object_dotget_number(conf_obj, param_name);
			snprintf(param_name, sizeof param_name, "radio_%i.rssi_offset", i);
			rfconf.rssi_offset = (float)json_object_dotget_number(conf_obj, param_name);
			snprintf(param_name, sizeof param_name, "radio_%i.type", i);
			str = json_object_dotget_string(conf_obj, param_name);
			if (!strncmp(str, "SX1255", 6)) {
				rfconf.type = LGW_RADIO_TYPE_SX1255;
			} else if (!strncmp(str, "SX1257", 6)) {
				rfconf.type = LGW_RADIO_TYPE_SX1257;
			} else {
				log_msg("WARNING: invalid radio type: %s (should be SX1255 or SX1257)\n", str);
			}
			snprintf(param_name, sizeof param_name, "radio_%i.tx_enable", i);
			val = json_object_dotget_value(conf_obj, param_name);
			if (json_value_get_type(val) == JSONBoolean) {
				rfconf.tx_enable = (bool)json_value_get_boolean(val);
			} else {
				rfconf.tx_enable = false;
			}
			log_msg("INFO: radio %i enabled (type %s), center frequency %u, RSSI offset %f, tx enabled %d\n", i, str, rfconf.freq_hz, rfconf.rssi_offset, rfconf.tx_enable);
		}
		/* all parameters parsed, submitting configuration to the HAL */
		if (lgw_rxrf_setconf(i, rfconf) != LGW_HAL_SUCCESS) {
			log_msg("WARNING: invalid configuration for radio %i\n", i);
		}
	}

	/* set configuration for Lora multi-SF channels (bandwidth cannot be set) */
	for (i = 0; i < LGW_MULTI_NB; ++i) {
		memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
		snprintf(param_name, sizeof param_name, "chan_multiSF_%i", i); /* compose parameter path inside JSON structure */
		val = json_object_get_value(conf_obj, param_name); /* fetch value (if possible) */
		if (json_value_get_type(val) != JSONObject) {
			log_msg("INFO: no configuration for Lora multi-SF channel %i\n", i);
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
			log_msg("INFO: Lora multi-SF channel %i disabled\n", i);
		} else  { /* Lora multi-SF channel enabled, will parse the other parameters */
			snprintf(param_name, sizeof param_name, "chan_multiSF_%i.radio", i);
			ifconf.rf_chain = (uint32_t)json_object_dotget_number(conf_obj, param_name);
			snprintf(param_name, sizeof param_name, "chan_multiSF_%i.if", i);
			ifconf.freq_hz = (int32_t)json_object_dotget_number(conf_obj, param_name);
			// TODO: handle individual SF enabling and disabling (spread_factor)
			log_msg("INFO: Lora multi-SF channel %i>  radio %i, IF %i Hz, 125 kHz bw, SF 7 to 12\n", i, ifconf.rf_chain, ifconf.freq_hz);
		}
		/* all parameters parsed, submitting configuration to the HAL */
		if (lgw_rxif_setconf(i, ifconf) != LGW_HAL_SUCCESS) {
			log_msg("WARNING: invalid configuration for Lora multi-SF channel %i\n", i);
		}
	}

	/* set configuration for Lora standard channel */
	memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
	val = json_object_get_value(conf_obj, "chan_Lora_std"); /* fetch value (if possible) */
	if (json_value_get_type(val) != JSONObject) {
		log_msg("INFO: no configuration for Lora standard channel\n");
	} else {
		val = json_object_dotget_value(conf_obj, "chan_Lora_std.enable");
		if (json_value_get_type(val) == JSONBoolean) {
			ifconf.enable = (bool)json_value_get_boolean(val);
		} else {
			ifconf.enable = false;
		}
		if (ifconf.enable == false) {
			log_msg("INFO: Lora standard channel %i disabled\n", i);
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
				case  7: ifconf.datarate = DR_LORA_SF7;  break;
				case  8: ifconf.datarate = DR_LORA_SF8;  break;
				case  9: ifconf.datarate = DR_LORA_SF9;  break;
				case 10: ifconf.datarate = DR_LORA_SF10; break;
				case 11: ifconf.datarate = DR_LORA_SF11; break;
				case 12: ifconf.datarate = DR_LORA_SF12; break;
				default: ifconf.datarate = DR_UNDEFINED;
			}
			log_msg("INFO: Lora std channel> radio %i, IF %i Hz, %u Hz bw, SF %u\n", ifconf.rf_chain, ifconf.freq_hz, bw, sf);
		}
		if (lgw_rxif_setconf(8, ifconf) != LGW_HAL_SUCCESS) {
			log_msg("WARNING: invalid configuration for Lora standard channel\n");
		}
	}

	/* set configuration for FSK channel */
	memset(&ifconf, 0, sizeof ifconf); /* initialize configuration structure */
	val = json_object_get_value(conf_obj, "chan_FSK"); /* fetch value (if possible) */
	if (json_value_get_type(val) != JSONObject) {
		log_msg("INFO: no configuration for FSK channel\n");
	} else {
		val = json_object_dotget_value(conf_obj, "chan_FSK.enable");
		if (json_value_get_type(val) == JSONBoolean) {
			ifconf.enable = (bool)json_value_get_boolean(val);
		} else {
			ifconf.enable = false;
		}
		if (ifconf.enable == false) {
			log_msg("INFO: FSK channel %i disabled\n", i);
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
			else if (bw <= 7800)   ifconf.bandwidth = BW_7K8HZ;
			else if (bw <= 15600)  ifconf.bandwidth = BW_15K6HZ;
			else if (bw <= 31200)  ifconf.bandwidth = BW_31K2HZ;
			else if (bw <= 62500)  ifconf.bandwidth = BW_62K5HZ;
			else if (bw <= 125000) ifconf.bandwidth = BW_125KHZ;
			else if (bw <= 250000) ifconf.bandwidth = BW_250KHZ;
			else if (bw <= 500000) ifconf.bandwidth = BW_500KHZ;
			else ifconf.bandwidth = BW_UNDEFINED;

			log_msg("INFO: FSK channel> radio %i, IF %i Hz, %u Hz bw, %u bps datarate\n", ifconf.rf_chain, ifconf.freq_hz, bw, ifconf.datarate);
		}
		if (lgw_rxif_setconf(9, ifconf) != LGW_HAL_SUCCESS) {
			log_msg("WARNING: invalid configuration for FSK channel\n");
		}
	}
	json_value_free(root_val);
	return 0;
}

/* -------------------------------------------------------------------------- */
/* --- MAC OSX Extensions  -------------------------------------------------- */

#ifdef __MACH__
int clock_gettime(int clk_id, struct timespec* t) {
	(void) clk_id;
	struct timeval now;
    int rv = gettimeofday(&now, NULL);
    if (rv) return rv;
    t->tv_sec  = now.tv_sec;
    t->tv_nsec = now.tv_usec * 1000;
    return 0;
}
#endif

double difftimespec(struct timespec end, struct timespec beginning) {
	double x;

	x = 1E-9 * (double)(end.tv_nsec - beginning.tv_nsec);
	x += (double)(end.tv_sec - beginning.tv_sec);

	return x;
}
