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
