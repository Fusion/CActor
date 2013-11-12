# CACTOR

                    :`:/:. -`       `/:`    
                    +Ndyshdy`    `.-myos/`.     CActor is a very simple C library that
                   :Nyy+++omy     .dyo++h+      aims  to   offer  the  simplicity   of 
                   hmyN+oyssM.    `myo++os``    actors-based concurrency  as seen more
                 ` NhdN++d+oM/``  -msoo++d-     traditionally in functional languages.
                 ./MyyNsomosM+-   :ds++++d      
      .` -:-`-    -Myymo+mooM/    /ds+os+d`     Threads may be used under the hood, but
       -yhoos:    -Myym+om++M:.   +ds+o++d`     application  developers need not  worry
       +ms+++s    -Myym+sdosN/` : syo++++d:`    about them: the library offers a simple
     . mds+++y-` `-Mysd+yhsoN+::+yyo++s+om`     API that  provides  a  basic  messaging
     `+Myo+o+h`  `/Mysd+sh++Nsoosso++so+od `    interface ("mailbox") to exchange  data
      /Msooy+h.   `Msydoodo+Ns+++os+oooomy-.    between actors.
      +Nyoo++y:   `Myhds+dsoNsooooossyhmm.      
    `-+Myo++syy-.`.mshhoodoomyyyhhhhyo+:-`      This design goes beyond data exchange,
     `+Myso+ooss+//Nyhhoomo+mds+:--`````        obviously,  as  it  also  lets  actors
      -Myyso+++soosMyhdsoN++N.`.                synchronize  with each  other,  either
       Ndsoo++oo+++Nyhd+omosN`                  by  accepting  messages  or   sleeping
    ...odddhysssoooNyhh+odsoN-.                 for desired amounts of time.
        `-/ssyhddddMhdh+sd++N-                  
          ``   `.+yMhmy+yd+oM.                  Under the hood, actors yield  regularly
                  :Mymy+ymsoM:`                 to  other  actors and tasks,  providing
                  `MymyohN++M.                  scheduling that is as fair as possible.
                  :NymhoyN+oM-                  
                   Nydh+sM+oM-                  
                   Nhhm+oMooM.                  
                   NmmNssMyoM.                  
                 `/NMMNdmMmdM:`                 
                :hhhyso/:--:/os                 
           `...........``..  .......         

# API

## ca_actor_t* ca_spawn(void*(*fn)(void*))

Pass a function to this call. An actor, running that function, will be spawned.

That actor's information will be returned.

## void ca_send(ca_actor_id_t id, unsigned long type, void* data, size_t data_size)

Send a message of type 'type' to the actor identified by the identifier 'id'

This identifier can be obtained using this macro: `ACTOR_ID(< ca_actor_t* >)`

The message can be any blob type, including char*.

Example:

    ca_actor_t* actor = ca_spawn(my_function);
    char msg[] = "Hello!";
    ca_send(ACTOR_ID(actor), 1, msg, sizeof(msg));

## ca_msg_t* ca_receive()

Receive a message and return information such as its content and type.

Block if no message is available.

A timeout parameter may be added in future implementations.

## void ca_release_msg(ca_msg_t* ca_msg)

Delete message retrieved using ca_receive()

When sending a message, your original data was duplicated for safekeeping.

This is the data we are now discarding along with its associated information.

## void ca_reply(ca_msg_t* msg, unsigned long type, void* data, size_t data_size)

A convenience function for replying to a message retrieved using ca_receive()

Do not forget to assign ca_receive()'s message and delete it after replying if you
are compositing these functions.

## ca_sleep(long milliseconds)

Sometimes you may wish for one of your actors to wait for a few seconds.

Pass a duration, in milliseconds, same kind that the regular sleep() call takes.

Here is an interesting fact about this function:

It is thread-safe *and* it will return prematurely if another actor sends a message
to the current actor.

## void ca_join()

Typically, you would call this function in the main function. It will block until
all actors have been released from duty.

# Missing

1. A lot!
2. Arguments! Currently, actors cannot be passed arbitrary arguments. Tsk tsk.
3. `ca_broadcast()` to send a message to all available actors

Number 3 is tricky to implement: all actors known at the time the message was sent
need to act upon that message.

Note: "known" not "up."

# Technical blather

## Debugging

Set `DEBUG_LOCKING` to 1 and CActor will become extremely verbose and display the
state of its mutexes as it accesses them. Debug output goes to stdout.

Set `DEBUG_ACTORS_LIST` to 1 and CActor will display its internal actors list
every time if reads it. Debug output goes to stdout.

If your code crashes while using CActor, a combination of gcc's '-g' flag and gdb
works well, even with the threading layer used under the hood.

Of course, if you find bugs in CActor, please let me know.

A simple debug command-line:

    gdb cactor_test core

## Flow and Locking

### About

The diagrams below show how actors are spawned. Actors rely on the pthreads library to
run in parallel. This fact is made entirely opaque to developers using CActor's API.

Various locks exist and one must be cautious to avoid deadlocks.

Locks List:

- 1 * actor list mutex
- 1 * message list mutex
- _nb_actors_ * actor condition mutex

### Diag.1: Main Thread

    main -- ca_spawn() -- ca_new_actor_() -- alloc actor
                                 |                |
                                 |           ca_actor->up = 0
                                 |
                          pthread_create() -------------------- /To Diag.2/
                                 |
                          ca_add_actor_to_list_() -- [guard actor list]
                                                            |
                                                     allocate actor node
                                                            |
                                                     add node to actor list
                                                            |
                                                     [leave actor list]

### Diag.2: A Receiver Thread

    /From Diag.1/
         \
    ca_actor_wrapper_() -- ca_wait_for_actor_known_()
                                      |
                                    /fn()/ -- ca_receive() ----- ca_get_thread_info_() ----- [guard actor list]
                                      |             |                  |                           |
                           ca_delete_actor_() ca_release_msg()   ca_actor->up = 1            find actor in node list
                                                                       |                           |
                                                                 [guard actor condition]     [leave actor list]
                                                                       |
                                                                       |
                                                                 ca_dequeue_msg_() ----- [guard msg list]
                                                                       |                       |
                                                          +------------?                 find, remove msg from list
                                                 (wait for condition)  |                       |
                                                          |            |                 [leave msg list]
                                                 ca_dequeue_msg_()     |
                                                          +------------+
                                                                       |
                                                                 [leave actor condition]


[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/Fusion/cactor/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

