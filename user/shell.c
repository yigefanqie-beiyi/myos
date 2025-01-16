#include "shell.h"
#include "global.h"
#include "stdint.h"
#include "string.h"
#include "syscall.h"
#include "stdio.h"
#include "file.h"
#include "debug.h"

#define cmd_len    128 //���֧��128���ַ�
#define MAX_ARG_NR 16  //��������֧��15������

char cmd_line[cmd_len] = {0};
char cwd_cache[64] = {0}; //Ŀ¼�Ļ��� ִ��cd���ƶ�������Ŀ¼ȥ
char* argv[MAX_ARG_NR];   //����

char final_path[MAX_PATH_LEN];

//�̶������ʾ
void print_prompt(void)
{
    printf("[tomato@localhost %s]$ ",cwd_cache);
}

//�Ӽ��̻�������������count�ֽڵ�buf
void readline(char* buf,int32_t count)
{
    ASSERT(buf != NULL && count > 0);
    char* pos = buf;
    //Ĭ��û�е��س��Ͳ�ֹͣ ��һ��һ���ֽڶ�
    while(read(stdin_no,pos,1) != -1 && (pos - buf) < count)
    {
        switch(*pos)
        {
            //����
            case 'l'-'a':
                *pos = 0;
                clear();
                print_prompt();
                printf("%s",buf);  //�Ѹոռ�����ַ���ӡ����
                break;
            
            //�������
            case 'u'-'a':
                while(buf != pos)
                {
                    putchar('\b');
                    *(pos--) = 0;
                }
                break;
                
            //������Ļس�һ��
            case '\n':
            case '\r':
                *pos = 0;
                putchar('\n');
                return;
            
            case '\b':
                if(buf[0] != '\b') //��ֹɾ�����Ǳ����������Ϣ
                {
                    --pos;
                    putchar('\b');
                }
                break;
            
            default:
                putchar(*pos);
                ++pos;
        }
    }
    printf("readline: cant fine entry_key in the cmd_line,max num of char is 128\n");
}

//����������ַ� tokenΪ�ָ����
int32_t cmd_parse(char* cmd_str,char** argv,char token)
{
    ASSERT(cmd_str != NULL);
    int32_t arg_idx = 0;
    
    //��ʼ��ָ������
    while(arg_idx < MAX_ARG_NR)
    {
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char* next = cmd_str;
    int32_t argc = 0;
    while(*next)
    {
        //�����ָ���
        while(*next == token)	++next;
        if(*next == 0)	break;	//��������ַ���ĩβ�����˳�ѭ��

        //���浱ǰ��������ʼλ��
        argv[argc] = next;     //������ô�� �����ﶼ�����ַ��ĵط� ��argc��ʾĿǰ���ڵڼ��������ַ���
        
        //������ǰ����
        while(*next && *next != token)  //Ҫ��������Ϊ0 ���ߵ��˷ָ����
            ++next;
        
        // �൱���ǣ���������ַ�����Ŀո�ȫ���滻���˽�����
        if(*next)
            *(next++) = 0; //�������������ַ�����ĩβ0 �ָ����λ�øպô���Ϊ0 �����ַ����н���ĩβ��
                        //���һ������������Ȼ��'\0'
        if(argc > MAX_ARG_NR)
            return -1;
        
        ++argc;        
    }
    return argc;        //���ٸ�����
}

void my_shell(void)
{
    cwd_cache[0] = '/';
    cwd_cache[1] = 0;
    int argc = -1;
    while(1)
    {
        print_prompt();
        memset(cmd_line,0,cmd_len);
        memset(final_path,0,MAX_PATH_LEN);
        memset(argv,0,sizeof(char*) * MAX_ARG_NR);
        readline(cmd_line,cmd_len);
        if(cmd_line[0] == 0)
            continue;
            
        argc = -1;  
        argc = cmd_parse(cmd_line,argv,' ');
        if(argc == -1)
        {
            printf("num of arguments exceed %d\n",MAX_ARG_NR);
            continue;
        }
        if(!strcmp("ls",argv[0]))
            buildin_ls(argc,argv);
        else if(!strcmp("pwd",argv[0]))
            buildin_pwd(argc,argv);
        else if(!strcmp("ps",argv[0]))
            buildin_ps(argc,argv);
        else if(!strcmp("cd",argv[0]))
        {
            if(buildin_cd(argc,argv) != NULL)
            {
                memset(cwd_cache,0,MAX_PATH_LEN);
                strcpy(cwd_cache,final_path);
            }
        }
        else if(!strcmp("clear",argv[0]))
            buildin_clear(argc,argv);
        else if(!strcmp("mkdir",argv[0]))
            buildin_mkdir(argc,argv);
        else if(!strcmp("rmdir",argv[0]))
            buildin_rmdir(argc,argv);   
        else if(!strcmp("rm",argv[0]))
            buildin_rm(argc,argv); 
        else
            printf("external command\n");
    }
    PANIC("my_shell: should not be here");
}

