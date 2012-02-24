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

#define SAMPLE_MSG_TYPE_HELLO	1
#define SAMPLE_MSG_TYPE_REPLY	2
#define NUM_MSGS				1500

void* pongfn(void* args) {
	printf("Starting pong\n");

	int i;
	for(i=0; i<NUM_MSGS; i++) {
		ca_msg_t* pingmsg = ca_receive();
		switch(pingmsg->type) {
			case SAMPLE_MSG_TYPE_HELLO: {
				ca_actor_id_t source_id = pingmsg->src_id;

				char txt[20];
				sprintf(txt, "PONG:%s", (char*)pingmsg->data);

				printf("PONG Received message type = %lu text = %s\n", pingmsg->type, (char*)pingmsg->data);
				ca_release_msg(pingmsg);

				printf("PONG sending %s\n", txt);
				ca_send(source_id, SAMPLE_MSG_TYPE_REPLY, txt, sizeof(txt));
				break;
			}
			default:
				ca_release_msg(pingmsg);
		}
	}

	printf("Pong actor is done.\n");
	return 0;
}

void* pingfn(void* args) {
	printf("Starting ping\n");

	ca_actor_t* pongactor = ca_spawn(pongfn);
	printf("Spawned pong actor id#%lu\n", (unsigned long)ACTOR_ID(pongactor));

	int i;
	for(i=0; i<NUM_MSGS; i++) {
		char txt[20];
		sprintf(txt, "Ping #%d", i);
		printf("Ping sending %s\n", txt);
		ca_send(ACTOR_ID(pongactor), SAMPLE_MSG_TYPE_HELLO, txt, sizeof(txt));
		// Force first two iterations to get reply before moving on
		// After that, go crazy!
		// So, in a nutshell, the only thing we know for sure is that we
		// will have ping..pong..ping..pong..and then randomness
		if(i<2) {
			ca_msg_t* pongmsg = ca_receive();
			switch(pongmsg->type) {
				case SAMPLE_MSG_TYPE_REPLY:
					printf("Ping Received message type = %lu text = %s\n", pongmsg->type, (char*)pongmsg->data);
					break;
			}
			ca_release_msg(pongmsg);			
		}
	}

	for(i=0; i<NUM_MSGS-2; i++) {
		ca_msg_t* pongmsg = ca_receive();
		switch(pongmsg->type) {
			case SAMPLE_MSG_TYPE_REPLY:
				printf("Ping Received message type = %lu text = %s\n", pongmsg->type, (char*)pongmsg->data);
				break;
		}
		ca_release_msg(pongmsg);
	}

	printf("Ping actor is done.\n");
	return 0;
}

int main(int argc, char **argv) {
	ca_actor_t* pingactor = ca_spawn(pingfn);
	printf("Spawned ping actor id#%lu\n", (unsigned long)ACTOR_ID(pingactor));
	ca_join();
	printf("All actors are down. I'm done.\n");
	return 0;
}
