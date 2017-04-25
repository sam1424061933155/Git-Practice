
//
//  main.cpp
//  rush
//
//  Created by Rudy on 2014/10/28.
//  Copyright (c) 2014年 Rudy. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_INPUT_LENGTH 10001
#define MAX_PIPE_LENGTH 5
#define MAX_COMMAND_LENGTH 257
#define MAX_PATH_LENGTH 257
#define ORIGINAL_PATH "bin"
#define WR_FILE_MODE (O_CREAT|O_WRONLY|O_TRUNC)

using namespace std;

struct fdcounts
{
    int infd;
    int outfd;
    int counter;

    fdcounts(int infd, int outfd, int counter)
    {
        this->infd = infd;
        this->outfd = outfd;
        this->counter = counter;
    }
};

enum Sign
{
    VERTICAL_BAR,
    CLOSE_BRACKET,
    NONE
};

bool isReturn(char c)
{
    if (c == '\n' || c =='\r')
        return true;
    return false;
}

bool isDigit(char c)
{
    if ( (c - '0') >= 0 && (c - '0') <= 9 )
        return true;
    return false;
}

bool isSpace(char c)
{
    if (c == ' ' || c == '\t')
        return true;
    return false;
}

void welcomeTxt(int sockfd)
{
    write(sockfd, "****************************************\n", 41);
    write(sockfd, "** Welcome to the information server. **\n", 41);
    write(sockfd, "****************************************\n", 41);
    return;
}

void extractTelnetHeader(int sockfd)
{
    char c[30];
    //HARDCODE: telnet sends header of 30 characters
    if (read(sockfd, c, 30) != 30)
    {
        perror("extracting telnet header error");
    }
}

bool containSlash(char *cmd)
{
    int i = 0;
    while (cmd[i] != '\0')
    {
        if (cmd[i] == '/')
            return true;
        else
            i++;
    }
    return false;
}

void writeErrcmdToSock(int sockfd, char *cmd)
{
    int len = 0;
    write(sockfd, "Unknown command: [", 18);
    while (cmd[len++] != '\0');
    write(sockfd, cmd, len-1);
    write(sockfd, "]\n", 2);
}

/*/
 * Input
 *  inputBuff: the raw input string
 *
 * Output
 *  i: current position of file
 *  cmd: parsed command
 *  hasPipeSign: whether there is a '|' in command
 *  n: the number behind '|' sign
 */
void parseCmd(int &i, char *inputBuff, char *cmd, char *filename, Sign &sign, int &n)
{
    int posOfCmd = 0;
    int posOfFile = 0;
    char pipeLength[MAX_PIPE_LENGTH];
    while (!isReturn(inputBuff[i]) && i < MAX_INPUT_LENGTH)
    {
        if (inputBuff[i] == '|')     //parse'|'後面的數字
        {
            i++; //Move to the first digit
            sign = VERTICAL_BAR;
            int posOfPipeLength = 0;

            if (isSpace(inputBuff[i]) || isReturn(inputBuff[i]))
            {
                pipeLength[posOfPipeLength++] = '1';
            }
            else
            {
                while (isDigit(inputBuff[i]))
                {
                    pipeLength[posOfPipeLength++] = inputBuff[i++];
                }
            }
            pipeLength[posOfPipeLength] = '\0';
            break;
        }
        else if (inputBuff[i] == '>')    //parse寫入檔案的command，‘>'後面會出現檔案名稱，空白可以跳過，只要記錄檔案名稱
        {
            sign = CLOSE_BRACKET;

            while (isSpace(inputBuff[++i]));    

            while (!isSpace(inputBuff[i]) && !isReturn(inputBuff[i]))
            {
                filename[posOfFile++] = inputBuff[i++];
            }
        }
        else                            //parse一般command
        {
            cmd[posOfCmd++] = inputBuff[i++];
        }
    }
    cmd[posOfCmd] = '\0';
    filename[posOfFile] = '\0';

    if (sign == VERTICAL_BAR)
        n = atoi(pipeLength);     //將'|'後面數字轉為int
    else
        n = 0;
}

bool isCmdExist(char *path, char *cmd)
{
    char tmp[MAX_COMMAND_LENGTH];
    char cpyCmd[MAX_COMMAND_LENGTH];
    int pos = 0;
    while (!isSpace(cmd[pos]) && cmd[pos] != '\0')
    {
        cpyCmd[pos] = cmd[pos];
        pos++;
    }
    cpyCmd[pos] = '\0';

    if (strcmp(cpyCmd, "printenv") == 0 ||                  //strcmp() return 0 , means two content is equal
            strcmp(cpyCmd, "setenv") == 0 ||
            strcmp(cpyCmd, "exit") == 0 )
    {
        return true;
    }
    path = getenv("PATH");
    strcpy(tmp, path);                      //copy source to destination, souce is second parameter
    strcat(tmp, "/");
    strcat(tmp, cpyCmd);                    //把第二個參數字串的接到第一個參數的字串後面，然後回傳第一個參數
    int fd = open(tmp, O_RDONLY);           //O_RDONLY 只讀打開
    if (fd == -1)
    {
        return false;
    }
    close(fd);
    return true;
}

void recoverFromErrCmd(vector<fdcounts> &counterTable)
{
    for (vector<fdcounts>::iterator it = counterTable.begin();
            it != counterTable.end();
            it++)
    {
        (it->counter)++;
    }
}

/**
 * Return
 *     0 indicates OK
 *     Otherwise failure
 **/
int processCmd(char *path, char *cmd, int infd, int outfd)
{
    int totalarg, childpid;
    char *token;
    char *arglist[MAX_COMMAND_LENGTH];
    path = getenv("PATH");

    //Check whether there is a slash in cmd
    if (containSlash(cmd))
    {
        return -1;
    }

    //Initial total number of arguments
    totalarg = 0;
    //Parse arguments
    token = strtok(cmd, " ");   //split string into token ，把command和參數分開ex:removetag test.html
    while (token != NULL)
    {
        arglist[totalarg] = strdup(token);  //duplicate a string
        totalarg++;
        token = strtok(NULL, " ");
    }

    //Null terminated for execv()
    arglist[totalarg] = NULL;

    //SETENV, GETENV
    if (strcmp(arglist[0], "printenv") == 0)
    {
        if (arglist[1] != NULL)
        {
            path = getenv(arglist[1]);
        }
        else
        {
            path = getenv("PATH");
        }
        if (path != NULL)
        {
            int sizeOfPath = 0;
            while (path[sizeOfPath] != '\0')
            {
                sizeOfPath++;
            }
            write(outfd, path, sizeOfPath);
            //line feed
            write(outfd, "\n", 1);
            return 0;
        }
    }
    else if (strcmp(arglist[0], "setenv") == 0)
    {
        setenv(arglist[1], arglist[2], 1);
        return 0;
    }
    else if (strcmp(arglist[0], "exit") == 0)
    {
        return -1;
    }

    childpid = fork();
    if (childpid < 0)
    {
        perror("fork() error");
        return -1;
    }
    else if (childpid == 0)
    {
        //Change stdin, stdout
        if (infd != 0)
        {
            if (dup2(infd, 0) == -1 ) 
            {
                perror("change stdin to pipe error");
                exit(-1);
            }
        }
        if (outfd != 1)
        {
            if (dup2(outfd, 1) == -1)     //stdout指向outfd所開啟的新描述符，任何寫道stdout也會寫到新的描述符，複製後outfd會關閉，新的不會
            {
                perror("change stdout to pipe error");
                exit(-1);
            }
        }

        //EXEC
        strcat(path, "/");
        strcat(path, arglist[0]);
        if (execv(path, arglist) == -1)         //execv()用来执行参数path 字符串所代表的文件路径,execve()只需两个参数, 第二个参数利用数组指针来传递给执行文件.
        {
            perror("exec error");
        }
        exit(0);
    }
    else
    {
        int status = 0;
        while (waitpid(childpid, &status, 0) > 0);  //wait for process to change state
        if (status != 0)                            // A state change is considered to be: the child terminated; the child was stopped by a signal
        {                                           //or the child was resumed by a signal
            return -1;                              //return >0 meaning wait for the child whose process ID is equal to the value of pid.
        }
    }
    return 0;
}

int evalOperator(vector<fdcounts> &counterTable, int infd, int outfd, char *path, char *cmd, int sockfd)
{
    bool flag = false;

    vector<fdcounts>::iterator it = counterTable.begin();
    while (it != counterTable.end())
    {
        if (it->counter == 0)   //  檢查有沒有command pipeN為0還沒印出
        {
            flag = true;
            if (!isCmdExist(path, cmd))         //command不存在
            {
                writeErrcmdToSock(sockfd, cmd);
                recoverFromErrCmd(counterTable);
                return -1;
            }
            close(it->outfd);   //??
            int status = processCmd(path, cmd, it->infd, outfd);
            if (status == -1)
            {
                return -2;
            }
            close(it->infd);
            counterTable.erase(it);
        }
        else
        {
            it++;
        }
    }

    //Nothing pipe to this and nothing pipe to
    if (!flag)
    {
        if (!isCmdExist(path, cmd))
        {
            writeErrcmdToSock(sockfd, cmd);
            recoverFromErrCmd(counterTable);
            return -1;
        }
        //read from socket, write to socket
        int status = processCmd(path, cmd, infd, outfd);
        if (status == -1)
        {
            return -2;
        }
    } 
    return 0;
}

void runShell(int sockfd)
{
    char inputBuff[MAX_INPUT_LENGTH];
    vector<fdcounts> counterTable;
    char* path;
    setenv("PATH", ORIGINAL_PATH, 1);   //setenv(envname,envval,overwrite);如果envame已經存在overwirte！=0
    //redirect errmsg to socket
    if (dup2(sockfd, 2) == -1)
    {
        perror("change stderr to sock error");
        return;
    }

    while(1)
    {
        //Write prompt symbol
        write(sockfd, "% ", 2);  //2指的是兩個byte

        //Reading a line from socket
        int pos = 0;
        char c;
        while (read(sockfd, &c, 1) == 1)                 //用while把command讀進來讀到換行為止
        {
            inputBuff[pos++] = c;
            // \n is the last character of line that should be read
            if (c == '\n')
                break;
        }

        //Parsing out first command and pipe n
        int i = 0;
        while (!isReturn(inputBuff[i]))
        {
            int pipefd[2], pipeN;       //pipefd[2]一個for read，一個for wirte
            char cmd[MAX_COMMAND_LENGTH];
            char filename[MAX_COMMAND_LENGTH];
            bool pipeToSame = false;
            vector<fdcounts>::iterator pipeIt;
            Sign sign = NONE;

            //pass through spaces and tabs
            while (isSpace(inputBuff[i]))   
            {
                i++;
            }

            //parsing command
            parseCmd(i, inputBuff, cmd, filename, sign, pipeN);

            //Decrease every counter by 1
            for (vector<fdcounts>::iterator it = counterTable.begin();            //每讀一個command進來講counter-1(counter是紀錄該command的pipeN)
                    it != counterTable.end();
                    it++)
            {
                (it->counter)--;
                if (pipeN == it->counter)            // t.counter（已經讀進來command的pipeN）,pipeN(是目前新進command的pipeN)
                {                                    //經過相同行數，同時pipe
                    pipeToSame = true;
                    pipeIt = it;
                }
            }

            if (sign == VERTICAL_BAR)
            {
                //Execute whose counters reach 0
                int infd = sockfd;
                int outfd;
                bool flag = false;
                int err_cmd = 0;

                //Reserve pipe
                if (pipeToSame)
                {
                    outfd = pipeIt->outfd;
                }
                else
                {
                    if (pipe(pipefd) < 0)               //pipe() creates a pipe, a unidirectional data channel that can be used for interprocess communication. 
                    {                                   //The array pipefd is used to return two file descriptors referring to the ends of the pipe.
                        perror("Pipe error!");          //pipefd[0] refers to the read end of the pipe. pipefd[1] refers to the write end of the pipe.
                        return;
                    }
                    outfd = pipefd[1];      
                }

                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    if (!pipeToSame)
                    {
                        close(pipefd[0]);
                        close(pipefd[1]);
                    }
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }

                if (!pipeToSame)
                {
                    counterTable.push_back(fdcounts(pipefd[0], pipefd[1], pipeN));      //countertable 放資料進去的地方
                }
            }
            else if (sign == CLOSE_BRACKET)
            {
                //exec
                int infd = sockfd;
                int outfd = open(filename, WR_FILE_MODE, S_IWUSR|S_IRUSR);   //S_IWUSR =Set write rights for the owner to true.
                bool flag = false;                                           //S_IRUSR = Set read rights for the owner to true.
                int err_cmd = 0;

                if (outfd == -1)
                {
                    perror("open file error");
                    break;
                }
                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }
                close(outfd);           // 在evaloperator->processcmd就執行完，最後把出口關掉
            }
            else
            {
                //exec
                int infd = sockfd;
                int outfd = sockfd;
                bool flag = false;
                int err_cmd = 0;

                err_cmd = evalOperator(counterTable, infd, outfd, path, cmd, sockfd);
                if (err_cmd == -1)
                {
                    break;
                }
                else if (err_cmd == -2)
                {
                    return;
                }
            }
        }
    }

}

int main (int argc, char *argv[])
{
    int sockfd, newsockfd, childpid;
    if (argc != 2)
    {
        printf("Usage: %s <port num>\n", argv[0]);
        return 1;
    }

    int TELNET_TCP_PORT = atoi(argv[1]);
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;    //sockaddr_in是一種特殊資料型態有sin_family,sin_port,sin_addr。sin_addr下更有s_addr
    signal(SIGCHLD, SIG_IGN);               // Silently (and portably) reap children. 

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)  //建立 socket 描述符 ,AF_INET：使用 IPv4,SOCK_STREAM：使用 TCP 協議
    {
        perror("server cannot open socket");
        return -1;
    }

    bzero((char*)&serv_addr, sizeof(serv_addr));    //将参数s 所指的内存区域前n 个字节，全部设为零值
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  //host to net long int 32位將32位的主機字節順序轉化為32位的網絡字節順序

    serv_addr.sin_port = htons(TELNET_TCP_PORT);

    if(bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("server cannot bind");
        return -1;
    }
    listen(sockfd, 5);
    while (1)
    {
        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("server cannot accept");
            return -1;
        }
        if ((childpid = fork()) < 0)        //fork(fork一次回有兩個return: parent and child)完會產生兩個process複製當前狀況，病各自去執行下列code
        {                                   //return值＝0，為child，他把多餘的sockfd（沒用到）刪除，而parent(return值>0)把newsockfd刪掉
            perror("server fork error");    //因為會複製當前狀況所以parent和child都有各自的sockfd和newsockfd
            return -1;
        }
        else if (childpid == 0)
        {
            //child
            //close original socket
            close(sockfd);
            welcomeTxt(newsockfd);
            runShell(newsockfd);
            exit(0);
        }
        else
        {
            //parent
            close(newsockfd);
        }
    }
    return 0;
}
