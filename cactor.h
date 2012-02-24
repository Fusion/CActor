/* 
 * This file is part of the CActor library.
 *  
 * Copyright (c) 2012 Chris Ravenscroft
 *  
 * This code is dual-licensed under the terms of the Apache License Version 2.0 and
 * the terms of the General Public License (GPL) Version 2.
 * You may use this code according to either of these licenses as is most appropriate
 * for your project on a case-by-case basis.
 * 
 * The terms of each license can be found in the root directory of this project's repository as well as at:
 * 
 * * http://www.apache.org/licenses/LICENSE-2.0
 * * http://www.gnu.org/licenses/gpl-2.0.txt
 *  
 * Unless required by applicable law or agreed to in writing, software
 * distributed under these Licenses is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See each License for the specific language governing permissions and
 * limitations under that License.
 */

// VERSION 0.1

#ifndef CA_DEFINES_H
#define CA_DEFINES_H
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define	DEBUG_LOCKING		0
#define DEBUG_ACTORS_LIST	0

// ---------------------------------------------------------

typedef pthread_t ca_actor_id_t;

typedef struct ca_actor_args ca_actor_args_t;

struct ca_actor {
	volatile int up;
	pthread_t thread;
	ca_actor_args_t* args;
	pthread_mutex_t thread_cond_mutex;
	pthread_cond_t  thread_cond;
};
typedef struct ca_actor ca_actor_t;

struct ca_actor_list_node;
typedef struct ca_actor_list_node ca_actor_list_node_t;
struct ca_actor_list_node {
	ca_actor_list_node_t* prev;
	ca_actor_list_node_t* next;
	ca_actor_t* actor;
};

// ---------------------------------------------------------
// Used to invoke an actor. Keep actor and associated
// function call in structure.
// ---------------------------------------------------------
struct ca_actor_args {
	ca_actor_t* ca_actor;
	void*(*fn)(void*);
};

// ---------------------------------------------------------

struct ca_msg {
	ca_actor_id_t dest_id;
	ca_actor_id_t src_id;
	unsigned long type;
	void* data;
	size_t data_size;
};
typedef struct ca_msg ca_msg_t;

struct ca_msg_list_node;
typedef struct ca_msg_list_node ca_msg_list_node_t;
struct ca_msg_list_node {
	ca_msg_list_node_t* next;
	ca_msg_t* msg;
};

#define PDEBUG(txt, name) printf("- LINE_%u:%s:%lu: %s\n", __LINE__, name, (unsigned long)pthread_self(), txt)
#if DEBUG_LOCKING == 1
#define GUARD_SECTION(name, id) PDEBUG("Attempt to guard", name); pthread_mutex_lock(&id); PDEBUG("Guarding", name);
#define LEAVE_SECTION(name, id) PDEBUG("Attempt to stop guarding", name); pthread_mutex_unlock(&id); PDEBUG("Stopped guarding", name);
#else
#define GUARD_SECTION(name, id) pthread_mutex_lock(&id);
#define LEAVE_SECTION(name, id) pthread_mutex_unlock(&id);
#endif

#if DEBUG_ACTORS_LIST == 1
#define PDEBUG_ACTORS_LIST	{ \
	printf("Actors list dump:\n"); \
	ca_actor_list_node_t* debug_node; \
	for(debug_node = ca_actor_list_head; debug_node != 0; debug_node = debug_node->next) { \
		printf("Node: id == %lu\n", (unsigned long)debug_node->actor->id); \
	} \
	printf("------\n"); \
}
#else
#define PDEBUG_ACTORS_LIST
#endif

/*
 * Convert thread to actor id.
 * Not using a real id field means that pthread_create() can
 * be thought of atomically (since no post-call id assignment)
 */
#define ACTOR_ID(x) (ca_actor_id_t)(x->thread)

enum {
	MSG_PRUNE_ACTION = 1,
	MSG_RETRIEVE_ACTION
};

// ---------------------------------------------------------
// PUBLIC API
// ---------------------------------------------------------

void ca_release_msg(ca_msg_t* ca_msg);
ca_actor_t* ca_spawn(void*(*fn)(void*));
ca_msg_t* ca_receive();
void ca_send(ca_actor_id_t id, unsigned long type, void* data, size_t data_size);
void ca_reply(ca_msg_t* msg, unsigned long type, void* data, size_t data_size);
void ca_sleep(long milliseconds);
void ca_join();

#endif /* CA_DEFINES_H */
