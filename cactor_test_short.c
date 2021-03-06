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

#define NUM_MSGS				1500

void* pongfn(void* args) {
	int i;
	for(i=0; i<NUM_MSGS; i++) {
		ca_msg_t* pingmsg;
		ca_reply(pingmsg = ca_receive(), 1, "Pong", sizeof("Pong"));
		ca_release_msg(pingmsg);
	}

	return 0;
}

void* pingfn(void* args) {
	ca_actor_t* pongactor = ca_spawn(pongfn);

	int i;
	for(i=0; i<NUM_MSGS; i++) {
		printf("Ping\n");
		ca_send(ACTOR_ID(pongactor), 1, "Ping", sizeof("Ping"));
		ca_msg_t* pongmsg = ca_receive();
		printf("%s\n", (char*)pongmsg->data);
		ca_release_msg(pongmsg);			
	}

	return 0;
}

int main(int argc, char **argv) {
	(void)ca_spawn(pingfn);
	ca_join();
	printf("All done.\n");
	return 0;
}
