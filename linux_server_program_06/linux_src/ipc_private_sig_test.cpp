//Copyright(c) by yyyyshen 2021
//

#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

union semun
{
    int val;
    struct semid_ds* buf;
    unsigned short int* array;
    struct seminfo* __buf;
};

//PV操作封装，参数op为-1时为P操作，为1时执行V操作
void pv(int sem_id, int op)
{
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = op;
    sem_b.sem_flg = SEM_UNDO;
    semop(sem_id, &sem_b, 1);
}

void
test_ipc_private()
{
    //创建一个key为特殊IPC_PRIVATE，集内信号量数量为1，权限为0666的信号集
    int sem_id = semget(IPC_PRIVATE, 1, 0666);

    //将信号中第0个（也仅有这一个）信号量设置值为1
    union semun sem_un;
    sem_un.val = 1;
    semctl(sem_id, 0, SETVAL, sem_un);

    //fork本进程
    pid_t id = fork();
    //根据返回值判断父子进程，执行对应的逻辑
    if (id < 0)
        return;//调用失败了
    else if (id == 0)
    {
        //等于0是子进程，尝试获取父进程中创建的信号量
        printf("child try to get binary sem\n");
        //父子进程共享IPC_PRIVATE信号量
        pv(sem_id, -1);
        //模拟进入临界区
        printf("child get the sem and release it after 3s\n");
        sleep(3);
        //出临界区
        pv(sem_id, 1);
        exit(0);
    }
    else
    {
        //父进程，与子进程通过信号量竞争一段临界区
        printf("parent try to get binary sem\n");
        //sleep(1);//模拟让子进程先进入临界区
        pv(sem_id, -1);
        printf("parent get the sem and release it after 3s\n");
        sleep(3);
        pv(sem_id, 1);
    }

    //等待子进程结束，避免僵尸态
    waitpid(id, NULL, 0);

    //清理信号量
    semctl(sem_id, 0, IPC_RMID, sem_un);
}

int main(int argc, char* argv[])
{
    test_ipc_private();

    return 0;
}
