#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define TRUE 1
#define FALSE 0
#define TIME_QUANTUM 100
#define TIME_INTERVAL 100000 // 0.1초를 의미
#define TOTALFORK 10

struct real_data{
        pid_t pid;
        int CPU_time;
        int IO_time;
        int tick;
};
struct message{
        long msg_type;
        struct real_data data;
};

typedef struct Process{
    pid_t pid;
    int Burst_time;
    int IO_time;
    int waiting_time;  
}Process;
typedef struct Node {
	Process info;
	struct Node* prev;
    struct Node* next;
}Node;
typedef struct Deque {
	Node* head;
    Node* tail;
	int size;
}Deque;

Deque* create_Deque(){
    Deque* d = (Deque*)malloc(sizeof(Deque));
	d->size = 0;
	d->head = d->tail = NULL;
	return d;
}
Node* create_Node(pid_t pid, int Burst_time, int IO_time, int waiting_time) {
	Node* newNode = (Node*)malloc(sizeof(Node));
	newNode->info.pid = pid;
    newNode->info.Burst_time = Burst_time;
    newNode->info.IO_time = IO_time;
    newNode->info.waiting_time = waiting_time;
	return newNode;
}
int is_empty(Deque *d){
    return (d->size==0)?TRUE:FALSE;
}
void push_process(Deque* d, Node* pNode){
    if (is_empty(d)){
        d->head = pNode;
        d->tail = pNode;
    }
    else{
        pNode->next = d->head;
        d->head->prev = pNode;
        d->head = pNode;
    }
    d->size++;
}
Node* pop_process(Deque* d){
    Node* tempNode = d->tail;
    if (!is_empty(d)){
        if(tempNode->prev == NULL){
            d->tail = NULL;
            d->head = NULL;
        }
        else{
            d->tail = tempNode->prev;
            d->tail->next = NULL;
        }
        d->size--;
        tempNode->prev = NULL;
        tempNode->next = NULL;
    }
    return tempNode;
} // 큐에서 프로세스를 완전히 pop 하는 함수
Node* peak_process(Deque* d){
    Node* tempNode = d->tail;
    return tempNode;
} // 큐에서 프로세스를 pop 하기전 프로세스의 Burst time 체크하기 위해 peak 하는 함수
void print_Ready_Queue(Deque *d){
    if (d->size==0) {
        printf("------------Ready-Queue----------------\n");
        printf("              Empty.. \n");
        printf("---------------------------------------\n");
    }
    else {
        printf("--------------Ready-Queue--------------\n");
        Node* temp = d->head;
        while(temp != NULL){
            printf("Process : %5ld CPU : %5d I/O : %5d\n",(long)temp->info.pid, temp->info.Burst_time, temp->info.IO_time);
            temp = temp->next;
        }
        temp = d->head;
        printf("---------------------------------------\n");
    }
}
void print_Wait_Queue(Deque *d){
    if (d->size==0) {
        printf("--------------Wait-Queue---------------\n");
        printf("               Empty.. \n");
        printf("---------------------------------------\n");
    }
    else {
        printf("--------------Wait-Queue---------------\n");
        Node* temp = d->head;
        while(temp != NULL){
            printf("Process : %5ld CPU : %5d I/O : %5d\n",(long)temp->info.pid, temp->info.Burst_time, temp->info.IO_time);
            temp = temp->next;
        }
        temp = d->head;
        printf("---------------------------------------\n");
    }
}

int main() {
    key_t key=1500; 
    key_t key2=1600;

    int msqid = msgget(key,IPC_CREAT|0666);
    int msqid_child_to_mom = msgget(key2,IPC_CREAT|0666);

	pid_t pids, mypid;
	int runProcess, state;

    srand(time(NULL));

    Deque* Ready_queue;
    Deque* Wait_queue;
    Ready_queue = create_Deque();
    Wait_queue = create_Deque();
    mypid = getpid();
    
    // timer
    clock_t start;
    clock_t now;

    while(runProcess < TOTALFORK){ // 자식 프로세스 TOTALFORK(10)개 만큼 생성하기
        if (getpid() == mypid){
            pids = fork();
            if (pids>0){
                Node* pNode = create_Node(pids, (rand()%10000) + 5000, (rand()%10000) + 5000, 0);            
                push_process(Ready_queue, pNode);
                printf("pparent %ld, parent %ld, child %ld\n", (long)getppid(), (long)getpid(), (long)pids);
            }
        }
        runProcess++;
    }
    // 부모, 자식 pid 나누기
    if (pids < 0){
        return -1;
    }
    else if(pids == 0){ // child
        struct message rcvMsg;
        struct message sndMsg;

        int remaintime;
        int num_CPU = 1;
        int num_IO = 1;
        while(num_CPU){
            if(msgrcv(msqid,&rcvMsg,sizeof(struct real_data),0,0)==-1){
                printf("msgrcv failed\n");
                exit(0);
            }
            
            if(rcvMsg.data.pid == getpid()){
                sndMsg.data.tick = rcvMsg.data.tick;
                sndMsg.data.pid = rcvMsg.data.pid;

                if(rcvMsg.data.CPU_time > TIME_QUANTUM){
                    printf("=======================================\n");
                    printf("               Tick %d\n", rcvMsg.data.tick);
                    printf("=======================================\n");
                    printf("          Child %ld Fetched\n",(long)getpid());
                    remaintime = rcvMsg.data.CPU_time - TIME_QUANTUM;
                    sndMsg.data.CPU_time = remaintime;
                    sndMsg.data.IO_time = rcvMsg.data.IO_time;
                    
                    msgsnd(msqid_child_to_mom, &sndMsg, sizeof(struct real_data),0);
                }
                else{
                    remaintime = 0;
                    printf("=======================================\n");
                    printf("               Tick %d\n", rcvMsg.data.tick);
                    printf("=======================================\n");
                    printf("          Child %ld finished!\n",(long)getpid());
                    
                    sndMsg.data.CPU_time = remaintime;
                    sndMsg.data.IO_time = rcvMsg.data.IO_time;

                    msgsnd(msqid_child_to_mom, &sndMsg, sizeof(struct real_data),0);

                    num_CPU = 0;
                }  
            }
            else{
                printf("Child did not receive message!\n");
            }
        }
        
        /*
        struct message rcvIOmsg;
        struct message sndIOmsg;
        
        if(num_CPU == 0){
            while(num_IO){
                if(msgrcv(msqid,&rcvIOmsg,sizeof(struct real_data),0,0)==-1){
                    printf("msgrcv failed\n");
                    exit(0);
                }
                
                if(rcvIOmsg.data.pid == getpid()){
                    sndIOmsg.data.tick = rcvIOmsg.data.tick;
                    sndIOmsg.data.pid = rcvIOmsg.data.pid;

                    if(rcvIOmsg.data.IO_time > TIME_QUANTUM){
                        printf("=======================================\n");
                        printf("               Tick %d\n", rcvIOmsg.data.tick);
                        printf("=======================================\n");
                        printf("             Child %ld I/O\n",(long)getpid());
                        remaintime = rcvIOmsg.data.IO_time - TIME_QUANTUM;
                        sndIOmsg.data.CPU_time = rcvIOmsg.data.CPU_time;
                        sndIOmsg.data.IO_time = remaintime;

                        msgsnd(msqid_child_to_mom, &sndIOmsg, sizeof(struct real_data),0);
                    }
                    else{
                        remaintime = 0;
                        printf("=======================================\n");
                        printf("               Tick %d\n", rcvIOmsg.data.tick);
                        printf("=======================================\n");
                        printf("         Child %ld I/O finished!\n",(long)getpid());
                        
                        sndIOmsg.data.CPU_time = rcvIOmsg.data.CPU_time;
                        sndIOmsg.data.IO_time = remaintime;

                        msgsnd(msqid_child_to_mom, &sndIOmsg, sizeof(struct real_data),0);

                        exit(0);
                    }  
                }
                else{
                    printf("Child did not receive message!\n");
                    exit(0);
                }
            }
        }
        */

    }
    else{ // parent
        print_Ready_Queue(Ready_queue);
        int startbutton;
        int i;
        int tick = 0;
        struct message msg;
        struct message rcvMsg_from_child;

        struct message sndIOmsg;
        struct message rcvIOMsg;

        printf("Enter 1 for Start >>");
        scanf("%d",&startbutton);
        if (startbutton==1){

            start = clock();
            float time_interval = TIME_INTERVAL;

            while (Ready_queue->size != 0){
                now = clock();
                Node* peakNode = peak_process(Ready_queue); // time quantum과 remaining time 을 비교해 time interval 정함.
                
                int rm_CPUtime = peakNode->info.Burst_time;
                float tmp_time_interval = (rm_CPUtime < TIME_QUANTUM) ? rm_CPUtime : TIME_QUANTUM;
                tmp_time_interval = tmp_time_interval / TIME_QUANTUM * TIME_INTERVAL;

                if(now - start >= time_interval){
                    tick++;
                    // printf("tmp_Time_interval >> %.2f\n",tmp_time_interval);
                    // printf("Time interval >> %.2f\n",time_interval);
                    
                    Node* node = pop_process(Ready_queue);
                    msg.msg_type = 1;
                    msg.data.tick = tick;
                    msg.data.pid = node->info.pid;
                    msg.data.CPU_time = node->info.Burst_time;
                    msg.data.IO_time = node->info.IO_time;

                    if(msgsnd(msqid,&msg,sizeof(struct real_data),0)==-1){
                        printf("msgsnd failed\n");
                        exit(0);
                    }

                    if(msgrcv(msqid_child_to_mom,&rcvMsg_from_child,sizeof(struct real_data),0,0)!=-1){
                        // printf("Received Msg From child %ld\n",(long)rcvMsg_from_child.data.pid);
                        if (rcvMsg_from_child.data.CPU_time == 0){
                            time_interval = tmp_time_interval;
                            node = create_Node(rcvMsg_from_child.data.pid, rcvMsg_from_child.data.CPU_time, rcvMsg_from_child.data.IO_time, 0);
                            push_process(Wait_queue, node);

                            print_Ready_Queue(Ready_queue);  
                            print_Wait_Queue(Wait_queue);
                        }
                        else{
                            node = create_Node(rcvMsg_from_child.data.pid, rcvMsg_from_child.data.CPU_time, rcvMsg_from_child.data.IO_time, 0);
                            push_process(Ready_queue, node);
                            
                            // printf("Repush Child %ld\n",(long)(rcvMsg_from_child.data.pid));
                            print_Ready_Queue(Ready_queue);
                            print_Wait_Queue(Wait_queue);
                        }
                    }
                    start = now;

                }
            }

            /*
            if (Ready_queue->size == 0){
                while (Wait_queue->size != 0){
                    now = clock();
                    Node* peakNode = peak_process(Wait_queue); // time quantum과 remaining time 을 비교해 time interval 정함.
                    
                    int rm_IOtime = peakNode->info.IO_time;
                    float tmp_time_interval = (rm_IOtime < TIME_QUANTUM) ? rm_IOtime : TIME_QUANTUM;
                    tmp_time_interval = tmp_time_interval / TIME_QUANTUM * TIME_INTERVAL;

                    if(now - start >= time_interval){
                        tick++;
                        // printf("tmp_Time_interval >> %.2f\n",tmp_time_interval);
                        // printf("Time interval >> %.2f\n",time_interval);
                        
                        Node* node = pop_process(Wait_queue);
                        sndIOmsg.msg_type = 1;
                        sndIOmsg.data.tick = tick;
                        sndIOmsg.data.pid = node->info.pid;
                        sndIOmsg.data.CPU_time = node->info.Burst_time;
                        sndIOmsg.data.IO_time = node->info.IO_time;

                        if(msgsnd(msqid,&sndIOmsg,sizeof(struct real_data),0)==-1){
                            printf("msgsnd failed\n");
                            exit(0);
                        }

                        if(msgrcv(msqid_child_to_mom,&rcvIOMsg,sizeof(struct real_data),0,0)!=-1){
                            // printf("Received Msg From child %ld\n",(long)rcvMsg_from_child.data.pid);
                            if (rcvIOMsg.data.IO_time == 0){
                                time_interval = tmp_time_interval;
                            }
                            else{
                                node = create_Node(rcvIOMsg.data.pid, rcvIOMsg.data.CPU_time, rcvIOMsg.data.IO_time, 0);
                                push_process(Wait_queue, node);
                                
                                // printf("Repush Child %ld\n",(long)(rcvMsg_from_child.data.pid));
                                print_Ready_Queue(Ready_queue);
                                print_Wait_Queue(Wait_queue);
                            }
                        }
                        start = now;

                    }
                }
            }
            */
        }
        return 0;
    }
}
