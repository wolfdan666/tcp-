#include<iostream>
#include"Client.h"

using namespace std;

Client::Client(){

    serverAddr.sin_family = PF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    sock = 0;
    pid = 0;

    isClientwork = true;
    epfd = 0;
}

void Client::Connect(){

    cout << "connect server : " << SERVER_IP << " : " << SERVER_PORT << endl;
    sock = socket(PF_INET,SOCK_STREAM,0);
    if(sock < 0 ){
        perror("sock error");
        exit(-1);
    }

    if(connect(sock,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0){
        perror("connect error");
        exit(-1);
    }
    if(pipe(pipe_fd) <0){
        perror("pipe error");
        exit(-1);
    }

    epfd = epoll_create(EPOLL_SIZE);
    if(epfd<0){
        perror("epfd error ");
        exit(-1);
    }
    addfd(epfd,sock,true);
    addfd(epfd,pipe_fd[0],true);

}

void Client::Close()
{
    if(pid){
        close(pipe_fd[0]);
        close(sock);
    }else{
        close(pipe_fd[1]);
    }

}

void Client::Start()
{
    static struct epoll_event  events[2];

    Connect();

    pid = fork();
    //子进程等待用户输入信息，将聊天信息写进管道，发送给父进程
    //父进程 使用epoll机制介绍服务端发来的消息，显示给用户，将子进程写入管道的信息读取出来，并发送客户端
    if(pid < 0){
        perror("fork error");
        exit(-1);
    }else if (pid == 0){ //child 执行进程  关闭读端
        close(pipe_fd[0]);

        cout << "Please input 'exit' to exit the chat room" << endl;
        cout<<"\\ + ClientID to private chat "<<endl;

        while (isClientwork)
        {
            memset(msg.content,0,sizeof(msg.content));
            fgets(msg.content,BUF_SIZE,stdin); //用户键盘输入

            if(strncasecmp(msg.content,EXIT,strlen(EXIT))==0){
                isClientwork =0;
            }
            else
            {
                memset(send_buf,0,BUF_SIZE);
                memcpy(send_buf,&msg,sizeof(msg));
                if(write(pipe_fd[1],send_buf,sizeof(send_buf))<0){//写入管道
                    perror("fork error");
                    exit(-1);
                }
            }
          } 
        }
        else
        {  //父进程  关闭写端
            close(pipe_fd[1]);
            while (isClientwork)
            {
                int epoll_events_count = epoll_wait(epfd,events,2,-1);
                for(int i =0; i<epoll_events_count; i++)
                {
                    memset(recv_buf,0,sizeof(recv_buf));
                   //服务端发来消息
                    if(events[i].data.fd == sock)
                    {
                        int ret = recv(sock,recv_buf,BUF_SIZE,0);
                        memset(&msg,0,sizeof(msg));
                        memcpy(&msg,recv_buf,sizeof(msg));
                        
                        if(ret==0){
                            cout << "Server closed connection: " << sock << endl;
                            close(sock);
                            isClientwork = 0;
                        }else
                        {
                            cout << msg.content<<endl;
                        }
                        
                    }
                    //子进程写入事件发生，父进程处理并发送服务器
                    else
                    {   //父进程管道中读取数据
                        int ret = read(events[i].data.fd,recv_buf,BUF_SIZE);
                        if(ret == 0){
                            isClientwork = 0;
                        }else
                        {
                            send(sock,recv_buf,sizeof(recv_buf),0);
                        }
                        
                    }
                    
                }
            }
            
        }
        
    Close();
}

