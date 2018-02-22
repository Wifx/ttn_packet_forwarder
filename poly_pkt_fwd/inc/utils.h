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
#ifndef _UTILS_H_
#define _UTILS_H_

#include "conf.h"
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>
#include "loragw_gps.h"

// EXPORTED DEFINED
#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))
#define STRINGIFY(x)	#x
#define STR(x)			STRINGIFY(x)
#define TRACE() 		fprintf(stderr, "@ %s %d\n", __FUNCTION__, __LINE__);

void log_set_output(char *log_output);
int log_msg(const char *format, ...);

struct gateway_conf{
	uint8_t serv_count;
	uint64_t lgwm;							/* Lora gateway MAC address */
	bool	serv_enable[MAX_SERVERS];
	bool 	serv_live[MAX_SERVERS];
	char 	serv_addr[MAX_SERVERS][64]; 	/* addresses of the server (host name or IPv4/IPv6) */
	char 	serv_port_up[MAX_SERVERS][8]; 	/* servers port for upstream traffic */
	char 	serv_port_down[MAX_SERVERS][8]; /* servers port for downstream traffic */
	int 	keepalive_time; 				/* send a PULL_DATA request every X seconds, negative = disabled */
	/* statistics collection configuration variables */
	unsigned stat_interval; 				/* time interval (in sec) at which statistics are collected and displayed */

	//TODO: This default values are a code-smell, remove.
	char 	ghost_addr[64]; 				/* address of the server (host name or IPv4/IPv6) */
	char 	ghost_port[8];					/* port to listen on */

	char 	monitor_addr[64];				/* address of the server (host name or IPv4/IPv6) */
	char 	monitor_port[8];				/* port to listen on */

	/* Reference coordinates, for broadcasting (beacon) */
	struct coord_s reference_coord;

	bool 	gps_fake_enable; 				/* fake coordinates override real coordinates */

	struct 	timeval push_timeout_half;
	struct 	timeval pull_timeout;

	bool 	fwd_valid_pkt;					/* packets with PAYLOAD CRC OK are forwarded */
	bool 	fwd_error_pkt;					/* packets with PAYLOAD CRC ERROR are NOT forwarded */
	bool 	fwd_nocrc_pkt; 					/* packets with NO PAYLOAD CRC are NOT forwarded */

	/* Control over the separate subprocesses. Per default, the system behaves like a basic packet forwarder. */
	bool 	gps_enabled;					/* controls the use of the GPS                      */
	bool 	beacon_enabled;					/* controls the activation of the time beacon.      */
	bool 	monitor_enabled;				/* controls the activation access mode.             */

	/* GPS configuration and synchronization */
	char 	gps_tty_path[64]; 				/* path of the TTY port GPS is connected on */
	int 	gps_tty_fd; 					/* file descriptor of the GPS TTY port */
	bool 	gps_active; 					/* is GPS present and working on the board ? */

	/* beacon parameters */
	uint32_t beacon_period; 				/* set beaconing period, must be a sub-multiple of 86400, the nb of sec in a day */
	uint32_t beacon_offset; 				/* must be < beacon_period, set when the beacon is emitted */
	uint32_t beacon_freq_hz; 				/* TX beacon frequency, in Hz */
	bool beacon_next_pps; 					/* signal to prepare beacon packet for TX, no need for mutex */

	/* Control over the separate streams. Per default, the system behaves like a basic packet forwarder. */
	bool 	upstream_enabled;				/* controls the data flow from end-node to server         */
	bool 	downstream_enabled;				/* controls the data flow from server to end-node         */
	bool 	ghoststream_enabled;			/* controls the data flow from ghost-node to server       */
	bool 	radiostream_enabled;			/* controls the data flow from radio-node to server       */
	bool 	statusstream_enabled;			/* controls the data flow of status information to server */

	/* auto-quit function */
	uint32_t autoquit_threshold; 			/* enable auto-quit after a number of non-acknowledged PULL_DATA (0 = disabled)*/

	/* Informal status fields */
	char platform[24];  					/* platform definition */
	char email[40];                			/* used for contact email */
	char description[64];                	/* used for free form description */
};

#define GATEWAY_CONF_INITIALIZER	\
{ \
	.lgwm = 0, \
	.serv_count = 0, \
    .keepalive_time = DEFAULT_KEEPALIVE, \
	.stat_interval = DEFAULT_STAT, \
	.ghost_addr = "127.0.0.1", \
	.ghost_port = "1914", \
	.monitor_addr = "127.0.0.1", \
	.monitor_port = "2008", \
	.push_timeout_half = {0, (PUSH_TIMEOUT_MS * 500)}, \
	.pull_timeout = {0, (PULL_TIMEOUT_MS * 1000)}, \
	.fwd_valid_pkt = true, \
	.fwd_error_pkt = false, \
	.fwd_nocrc_pkt = false, \
	.gps_enabled = false, \
	.beacon_enabled = false, \
	.monitor_enabled = false, \
	.upstream_enabled = true, \
	.downstream_enabled = true, \
	.ghoststream_enabled = false, \
	.radiostream_enabled = true, \
	.statusstream_enabled = true, \
	.beacon_period = 128, \
	.beacon_offset = 0, \
	.beacon_freq_hz = 0, \
	.beacon_next_pps = false, \
	.autoquit_threshold = 0, \
	.platform = DISPLAY_PLATFORM, \
	.email = "", \
	.description = "", \
}

int parse_gateway_configuration(const char * conf_file, struct gateway_conf *gtw_conf);
int parse_SX1301_configuration(const char * conf_file);


/* -------------------------------------------------------------------------- */
/* --- MAC OSX Extensions  -------------------------------------------------- */

struct timespec;

#ifdef __MACH__
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct timespec* t);
#endif

double difftimespec(struct timespec end, struct timespec beginning);

#endif /* _UTILS_H_ */
