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

#include "server.h"

void servers_init(struct servers *s){
	for(int i = 0; i < MAX_SERVERS; i++){
		s->s[i].parent = s;
		//pthread_mutex_init(&s->s[i].m, NULL);
		pthread_cond_init(&s->s[i].wait_started, NULL);
		s->s[i].state = SERVER_STOPPED;
	}
	pthread_mutex_init(&s->m, NULL);
	pthread_cond_init(&s->wait_one_started, NULL);
}

void servers_wait_one_started(struct servers *server){
	pthread_mutex_lock(&server->m);
	do{
		for(int i = 0; i < MAX_SERVERS; i++){
			if(server->s[i].state == SERVER_STARTED){
				pthread_mutex_unlock(&server->m);
				return;
			}
		}

		// wait for signal
		pthread_cond_wait(&server->wait_one_started, &server->m);
	}while(1);
}

void server_set_started(struct server *server){
	pthread_mutex_lock(&server->parent->m);
	if(server->state != SERVER_STARTED){
		server->state = SERVER_STARTED;
		pthread_cond_broadcast(&server->wait_started);
		pthread_cond_broadcast(&server->parent->wait_one_started);
	}
	pthread_mutex_unlock(&server->parent->m);
}

bool server_is_started(struct server *server){
	bool started;
	pthread_mutex_lock(&server->parent->m);
	started = server->state == SERVER_STARTED;
	pthread_mutex_unlock(&server->parent->m);
	return started;
}

void server_wait_started(struct server *server){
	pthread_mutex_lock(&server->parent->m);
	if(server->state == SERVER_STARTED){
		pthread_mutex_unlock(&server->parent->m);
		return;
	}

	// wait for signal
	pthread_cond_wait(&server->wait_started, &server->parent->m);
	pthread_mutex_unlock(&server->parent->m);
}
