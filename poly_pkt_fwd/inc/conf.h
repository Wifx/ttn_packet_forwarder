#ifndef _CONF_H_
#define _CONF_H_

#define MAX_SERVERS		    4 /* Support up to 4 servers, more does not seem realistic */

#define DEFAULT_KEEPALIVE	5	/* default time interval for downstream keep-alive packet */
#define DEFAULT_STAT		30	/* default time interval for statistics */
#define PUSH_TIMEOUT_MS		100
#define PULL_TIMEOUT_MS		200
#define GPS_REF_MAX_AGE		30	/* maximum admitted delay in seconds of GPS loss before considering latest GPS sync unusable */
#define FETCH_SLEEP_MS		10	/* nb of ms waited when a fetch return no packets */
#define BEACON_POLL_MS		50	/* time in ms between polling of beacon TX status */

//TODO: This default values are a code-smell, remove.
#define DEFAULT_SERVER		127.0.0.1 /* hostname also supported */
#define DEFAULT_PORT_UP		1780
#define DEFAULT_PORT_DW		1782

#define DEFAULT_SERVER		127.0.0.1 /* hostname also supported */
#define DEFAULT_PORT_UP		1780
#define DEFAULT_PORT_DW		1782
#define DEFAULT_KEEPALIVE	5	/* default time interval for downstream keep-alive packet */
#define DEFAULT_STAT		30	/* default time interval for statistics */
#define PUSH_TIMEOUT_MS		100
#define PULL_TIMEOUT_MS		200
#define FETCH_SLEEP_MS		10	/* nb of ms waited when a fetch return no packets */

#endif /* _CONF_H_ */
