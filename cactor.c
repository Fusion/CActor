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

#include "cactor.h"

int ca_lib_initialized = 0;

ca_actor_list_node_t* ca_actor_list_head = 0;
pthread_mutex_t thread_actor_list_mutex;

ca_msg_list_node_t* ca_msg_list_head = 0;
pthread_mutex_t thread_msg_list_mutex;

// Private forward declaration
ca_actor_t* ca_get_thread_info_(ca_actor_id_t id);

/*
 * Private
 * Add message to message list
 */
void ca_enqueue_msg_(ca_msg_t* ca_msg) {
	GUARD_SECTION("msgs-ca_enqueue_msg_", thread_msg_list_mutex)
	ca_msg_list_node_t* ca_msg_node =
		(ca_msg_list_node_t*)malloc(sizeof(ca_msg_list_node_t));
	ca_msg_node->msg = ca_msg;
	ca_msg_node->next = 0;
	if(ca_msg_list_head == 0) {
		ca_msg_list_head = ca_msg_node;
	}
	else {
		ca_msg_list_node_t* cur_node;
		for(cur_node = ca_msg_list_head; ; cur_node = cur_node->next) {
			if(cur_node->next == 0) {
				cur_node->next = ca_msg_node;
				break;
			}
		}
	}
	LEAVE_SECTION("msgs-ca_enqueue_msg_", thread_msg_list_mutex)
}

/*
 * Private
 * Depending on the value of action:
 *     MSG_RETRIEVE_ACTION -> return next message for current actor, or nothing
 *     MSG_PRUNE_ACTION -> delete all messages for current actor, return last one
 */
ca_msg_t* ca_dequeue_msg_(ca_actor_id_t id, int action) {
	GUARD_SECTION("msgs-ca_dequeue_msg_", thread_msg_list_mutex)
	ca_msg_t* ca_msg = 0;
	ca_msg_list_node_t* cur_node, * prev_node;
	prev_node = 0;
	for(cur_node = ca_msg_list_head; cur_node != 0; cur_node = cur_node->next) {
		if(cur_node->msg->dest_id == id) {
			ca_msg = cur_node->msg;
			if(prev_node == 0) {
				ca_msg_list_head = cur_node->next;
			}
			else {
				prev_node->next = cur_node->next;
			}
			free(cur_node);
			// If pruning, go through all messages.
			// If retrieving, simply return the msg found.
			if(action == MSG_RETRIEVE_ACTION) {
				break;
			}
		}
		prev_node = cur_node;
	}
	LEAVE_SECTION("msgs-ca_dequeue_msg_", thread_msg_list_mutex)
	return ca_msg;
}

/*
 * Private
 * Add actor information to actors list:
 * it's a double-linked list
 */
void ca_add_actor_to_list_(ca_actor_t* ca_actor) {
	GUARD_SECTION("actors-ca_add_actor_to_list_", thread_actor_list_mutex)
	ca_actor_list_node_t* ca_actor_node =
		(ca_actor_list_node_t*)malloc(sizeof(ca_actor_list_node_t));
	ca_actor_node->actor = ca_actor;
	ca_actor_node->next = 0;
	if(ca_actor_list_head == 0) {
		ca_actor_node->prev = 0;
		ca_actor_list_head = ca_actor_node;
	}
	else {
		ca_actor_list_node_t* cur_node;
		for(cur_node = ca_actor_list_head; ; cur_node = cur_node->next) {
			if(cur_node->next == 0) {
				ca_actor_node->prev = cur_node;
				cur_node->next = ca_actor_node;
				break;
			}
		}
	}
	LEAVE_SECTION("actors-ca_add_actor_to_list_", thread_actor_list_mutex)
}

/*
 * Private
 * Instantiate and return a new actor:
 * allocate memory, set mutex and condition to default values,
 * set up flag to 0
 */
ca_actor_t* ca_new_actor_() {
	ca_actor_t* ca_actor = (ca_actor_t*)malloc(sizeof(ca_actor_t));
	ca_actor->up = 0;
	pthread_mutex_init(&ca_actor->thread_cond_mutex, 0); // Default
	pthread_cond_init(&ca_actor->thread_cond, 0); // Default
	return ca_actor;
}

/*
 * Private
 * Delete actor
 * TODO First delete this actor's message entries
 */
void ca_delete_actor_(ca_actor_t* ca_actor) {
	(void)ca_dequeue_msg_(ACTOR_ID(ca_actor), MSG_PRUNE_ACTION);
	free(ca_actor);
}

/*
 * Private
 * Wait for an actor to be in the actors list
 */
void ca_wait_for_actor_known_(ca_actor_id_t id) {
	ca_actor_t* ca_actor = 0;
	// First wait for our actor to be in the actors list
	do {
		ca_actor = ca_get_thread_info_(id);
		if(ca_actor == 0) {
			sched_yield();
		}
	} while(ca_actor == 0);
}

/*
 * Private
 * Wait for an actor to be "realized"
 * the actor simply sets its information 'up' flag to 1
 * and that is we check (relinquishing our scheduler position
 * after every test)
 * Assumption: actor is already in actors list.
 */
void ca_wait_for_actor_up_(ca_actor_id_t id) {
	ca_actor_t* ca_actor = ca_get_thread_info_(id);
	for(;;) {
		if(ca_actor->up) {
			break;
		}
		sched_yield();
	}
}

/*
 * Private
 * This functions ALWAYS makes a copy of data. We may wish to add a lighter
 * version later.
 */
ca_msg_t* ca_new_msg_(ca_actor_id_t dest_id, unsigned long type, void* data, size_t data_size) {
	ca_actor_t* ca_actor = ca_get_thread_info_(pthread_self());
	ca_msg_t* ca_msg = (ca_msg_t*)malloc(sizeof(ca_msg_t));
	ca_msg->dest_id = dest_id;
	ca_msg->src_id  = ACTOR_ID(ca_actor);
	ca_msg->type = type;
	void* data_copy = (void*)malloc(sizeof(data_size));
	memcpy(data_copy, data, data_size);
	ca_msg->data = data_copy;
	ca_msg->data_size = data_size;
	return ca_msg;
}

/*
 * Private
 * Delete a message's payload then the message itself
 */
void ca_delete_msg_(ca_msg_t* ca_msg) {
	if(ca_msg->data != 0) {
		free(ca_msg->data);
	}
	free(ca_msg);	
}

void ca_release_msg(ca_msg_t* ca_msg) {
	ca_delete_msg_(ca_msg);
}

/*
 * Retrieve an actors' info based on its id:
 * look for actor through actors list, match on id
 */
ca_actor_t* ca_get_thread_info_(ca_actor_id_t id) {
	GUARD_SECTION("actors-ca_get_thread_info", thread_actor_list_mutex)
	ca_actor_t* ca_actor = 0;
	ca_actor_list_node_t* cur_node;
	for(cur_node = ca_actor_list_head; cur_node != 0; cur_node = cur_node->next) {
		if(ACTOR_ID(cur_node->actor) == id) {
			ca_actor = cur_node->actor;
			break;
		}
	}
	LEAVE_SECTION("actors-ca_get_thread_info", thread_actor_list_mutex)
	return ca_actor;
}

/*
 * Private
 * This function is what a thread executes first
 * This function will call the real function and perform
 * some cleanup upon return
 */
void* ca_actor_wrapper_(void* ca_args) {
	ca_actor_args_t* args = (ca_actor_args_t*)ca_args;
	ca_actor_t* ca_actor = args->ca_actor;
	// First, wait for actor to be in the actors list
	ca_wait_for_actor_known_(ACTOR_ID(ca_actor));
	//
	args->fn((void*)0);
	// We end up here when the actor's function returns
	// Time to clean up and leave
	free(args);
	ca_delete_actor_(ca_actor);
	return 0;
}

/*
 * Create a new actor:
 * create matching thread, assign id, add to actors list
 */
ca_actor_t* ca_spawn(void*(*fn)(void*)) {
	// Our friend here will be used to create our first thread.
	// By definition, this means that initializing states here
	// is safe until we create our first thread.
	if(!ca_lib_initialized) {
		ca_lib_initialized = 1;
		pthread_mutex_init(&thread_actor_list_mutex, 0);
		pthread_mutex_init(&thread_msg_list_mutex, 0);
	}

	ca_actor_t* ca_actor = ca_new_actor_();
	ca_actor_args_t* ca_args = (ca_actor_args_t*)malloc(sizeof(ca_actor_args_t)); // TODO: Free later
	ca_args->ca_actor = ca_actor;
	ca_args->fn = fn;
	ca_actor->args = ca_args;
	pthread_create(&(ca_actor->thread), 0, &ca_actor_wrapper_, (void*)ca_args);
	ca_add_actor_to_list_(ca_actor);	
	return ca_actor;
}

/*
 * Wait for a message for this actor:
 * lock, set own state to up (in case), wait for signal, unlock
 */
ca_msg_t* ca_receive() {
	ca_msg_t* ca_msg;
	ca_actor_t* ca_actor = ca_get_thread_info_(pthread_self());
	//printf("Retrieved %lu\n", (unsigned long)ca_actor->id);
	// TODO: Check that it;s to claim to be up while unguarded.
	ca_actor->up = 1;
	// Maybe we already have messages in the pipeline...
	// In that case we do not need to wait.
	GUARD_SECTION("1actor-ca_receive", ca_actor->thread_cond_mutex)
	ca_msg = ca_dequeue_msg_(ACTOR_ID(ca_actor), MSG_RETRIEVE_ACTION);
	while(ca_msg == 0) {
		// Looks like we will have to wait,,,
		pthread_cond_wait(&ca_actor->thread_cond, &ca_actor->thread_cond_mutex);
		ca_msg = ca_dequeue_msg_(ACTOR_ID(ca_actor), MSG_RETRIEVE_ACTION);
		if(ca_msg == 0) {
			sched_yield();
		}
	}
	LEAVE_SECTION("1actor-ca_receive", ca_actor->thread_cond_mutex)
	return ca_msg;
}

/*
 * Send message to actor identified by id:
 * Check that actor's thread is up, lock, signal, unlock
 */
void ca_send(ca_actor_id_t id, unsigned long type, void* data, size_t data_size) {
	// Wait for receiver to be realized
	ca_wait_for_actor_up_(id);
	// Prepare message
	ca_msg_t* ca_msg = ca_new_msg_(id, type, data, data_size);
	ca_enqueue_msg_(ca_msg);
	// Send signal: there is a message!
	ca_actor_t* ca_actor = ca_get_thread_info_(id);
	GUARD_SECTION("1actor", ca_actor->thread_cond_mutex)
	pthread_cond_signal(&ca_actor->thread_cond);
	LEAVE_SECTION("1actor", ca_actor->thread_cond_mutex)
}

/*
 * Reply to a message's sender
 * Invokes ca_send()
 */
void ca_reply(ca_msg_t* msg, unsigned long type, void* data, size_t data_size) {
	ca_send(msg->src_id, type, data, data_size);
}

void ca_sleep(long milliseconds) {
	ca_actor_t* ca_actor = ca_get_thread_info_(pthread_self());	
	// TODO: Check that it;s to claim to be up while unguarded.
	ca_actor->up = 1;	
	GUARD_SECTION("1actor-ca_sleep", ca_actor->thread_cond_mutex)
	struct timespec unlockAfter;
	struct timeval now;
	gettimeofday(&now, NULL);
	unlockAfter.tv_sec = now.tv_sec + milliseconds / 1000;
	unlockAfter.tv_nsec = now.tv_usec * 1000 + (milliseconds % 1000) * 1000 * 1000;
	pthread_cond_timedwait(&ca_actor->thread_cond, &ca_actor->thread_cond_mutex, &unlockAfter);
	LEAVE_SECTION("1actor-ca_sleep", ca_actor->thread_cond_mutex)
}

/*
 * Wait for all actors to have exited:
 * Loop through actors list,  pseudo-join(), next(), ...
 */
void ca_join() {
	ca_actor_list_node_t* cur_node;
	while(ca_actor_list_head != 0) {
		GUARD_SECTION("actors-ca_wait_for_actors_down", thread_actor_list_mutex)
		if(ca_actor_list_head != 0) {
			for(cur_node = ca_actor_list_head; cur_node != 0; cur_node = cur_node->next) {
				// BEWARE! Before these threads are currently marked as detached
				// rather than joinable (Are they really??? TODO: Check)
				// thread is may be reused. Yeah I don't think so...
				//pthread_join(cur_node->actor->thread, 0);
				int is_dead = (ESRCH == pthread_kill(cur_node->actor->thread, 0));
				if(is_dead) {
					pthread_mutex_destroy(&cur_node->actor->thread_cond_mutex);
					pthread_cond_destroy(&cur_node->actor->thread_cond);

					if(cur_node->next == 0 && cur_node->prev == 0) {
						ca_actor_list_head = 0;
					}
					else {
						if(cur_node->prev != 0) {
							cur_node->prev->next = cur_node->next;					
						}
						if(cur_node->next != 0) {
							cur_node->next->prev = cur_node->prev;
						}
					}
				}
			}
		}
		PDEBUG_ACTORS_LIST
		LEAVE_SECTION("actors-ca_wait_for_actors_down", thread_actor_list_mutex)

		sched_yield();
	}
}
