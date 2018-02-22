/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Wifx's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY WIFX "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL WIFX BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * */
#ifndef _SERVER_H_
#define _SERVER_H_

#include "conf.h"
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

enum server_state{
	SERVER_STOPPED = 0,
	SERVER_STARTED
};

struct servers;
struct server {
	enum server_state	state;
	pthread_cond_t		wait_started;
	struct servers		*parent;
};

struct servers {
	struct server		s[MAX_SERVERS];
	pthread_mutex_t 	m;
	pthread_cond_t		wait_one_started;
};

void servers_init(struct servers *servers);
void servers_wait_one_started(struct servers *server);
void server_set_started(struct server *server);
bool server_is_started(struct server *server);
void server_wait_started(struct server *server);

#endif /* _SERVER_H_ */
