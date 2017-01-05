/*
*  send.c - serial ymodem sender
*  
*  Copyright (c) 2012 April Jianzhong.Cui <jianzhong.cui@cs2c.com.cn>
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "serial.h"
#include "ymodem.h"
#include "common.h"

int init_packet(struct packet *p,int fd_tty,int fd_file,const char *path)
{
  struct packet *pkt;
  pkt = p;
  
  pkt->fd_tty = fd_tty;
  pkt->fd_file = fd_file;
  pkt->size = PACKET_128;
  pkt->head = 1;
  pkt->sno = 0x0;
  pkt->buf = malloc(pkt->size);
  pkt->crc1 = 0;
  pkt->crc2 = 0;
  
  pkt->file = malloc(sizeof(struct mcu_hex));
  pkt->file->path = path;
  pkt->file->name = strrchr(path, '/') + 1;
  if (strlen(pkt->file->name) > 128)
    return ERR_CODE_FILENAME;
  pkt->file->total_size = get_file_size(path);
  pkt->file->size = pkt->file->total_size;
  pkt->file->idx = NULL;
  pkt->file->idx_save = NULL;

  return ERR_CODE_NONE;
}

void packet_send(struct packet *p)
{
  int i, j, crc = 0;

  if (p->size == PACKET_128)
    put_char(p->fd_tty, SOH);
  else if (p->size == PACKET_1k)
    put_char(p->fd_tty, STX);
  
  put_char(p->fd_tty, p->sno);
  put_char(p->fd_tty, 255 - p->sno);
  p->sno++;
  
  if(p-> head == HEAD)
  {
    for(i = 0; i < p->size; i++)
    {
      put_char(p->fd_tty, p->buf[i]);
      crc = crc ^ (int)p->buf[i] << 8;
      for (j = 0; j < 8; j++)
        if (crc & 0x8000)
          crc = crc << 1 ^ 0x1021;
        else
          crc = crc << 1;
    }
    p->crc1 = (crc >> 8) & 0xff;
    p->crc2 = crc & 0xff;
    put_char(p->fd_tty, p->crc1);
    put_char(p->fd_tty, p->crc2);
  }else if (p->head == DATA)
  {
    for(i = 0; i < p->size; i++)
    {
      put_char(p->fd_tty, p->buf[i]);
      crc = crc ^ (int)p->buf[i] << 8;
      for (j = 0; j < 8; j++)
        if (crc & 0x8000)
          crc = crc << 1 ^ 0x1021;
        else
          crc = crc << 1;
    }
    p->crc1 = (crc >> 8) & 0xff;
    p->crc2 = crc & 0xff;
    put_char(p->fd_tty, p->crc1);
    put_char(p->fd_tty, p->crc2);
  }
}

int packet_xfr(struct packet *p)
{
  int fd,len,i,end;
  char c;
  int ret;
  unsigned int time;

  if(p->head == HEAD)
  {
    p->size = PACKET_128;
    p->buf = malloc(p->size);
    if (p->buf == NULL)
    {
      printf("malloc() error");
      return ERR_CODE_MALLOC;
    }
    /*fix me: if name or size is too long */
    printf("Sending File Header,Name:%s,size:%d\n",p->file->name,p->file->total_size);
    strncpy(p->buf, p->file->name, strlen(p->file->name)+1);
    sprintf(p->buf + strlen(p->file->name)+1, "%d", p->file->total_size);
    
    packet_send(p);
  }else if (p->head == DATA)
  {
    if (p->file->size > PACKET_1k)
      p->size = PACKET_1k;
    else
      p->size = PACKET_128;
    
    p->buf = realloc(p->buf, p->size);
    if (p->buf == NULL)
    {
      printf("realloc() error");
      return ERR_CODE_MALLOC;
    }

    end = 0;
    while(1)
    {
      if(end == 1) break;
      for(time = 0;;time++)
      {
        ret=read(p->fd_tty,&c,1);
		if(ret>0)
		  break;
		if(time>=TIME_TIMEOUT)
		  return ERR_CODE_NORSP;
		usleep(1);
      }
      switch (c)
      {
      case 'C': 
      case ACK: 
        printf("Sending File,CurAddr:%8d,Progress:%3d%%\n",\
          p->file->total_size-p->file->size,100*(p->file->total_size-p->file->size)/p->file->total_size);
        len = read(p->fd_file, p->buf, p->size);
        if (p->file->size <= 0)
        {
          i = 0;
          do
          {
            put_char(p->fd_tty, EOT);
            for(time = 0;;time++)
            {
              ret=read(p->fd_tty,&c,1);
	          if(ret>0)
	            break;
              if(time>=TIME_TIMEOUT)
	            return ERR_CODE_NORSP;
            }
            if(c == ACK)
              break;
            i++;
          }while(i < 3);
          for(time = 0;;time++)
          {
            ret=read(p->fd_tty,&c,1);
            if(ret>0)
              break;
            if(time>=TIME_TIMEOUT)
              return ERR_CODE_NORSP;
          }
          if (c == 'C' || c == 'c')
          {
            p->size = PACKET_128;
            for(i=0; i < p->size; i++)
              p->buf[i]=0;
            p->sno=0;
            packet_send(p);
            for(time = 0;;time++)
            {
              ret=read(p->fd_tty,&c,1);
	          if(ret>0)
	            break;
              if(time>=TIME_TIMEOUT)
	            return ERR_CODE_NORSP;
            }
            if (c == ACK)
            {
              end = 1;
              break;
            }
          }
        } 
        p->file->size -= len;
        packet_send(p); 
        break;
      case NAK:
        packet_send(p); 
        break;
      default: break;
      }
    }
  } else
  {
    printf("packet error\n");
    return ERR_CODE_OTHERS;
  }
  return ERR_CODE_NONE;
}

int ymodem_xfr(int fd_tty,int fd_file,const char *path)
{
  struct packet pkt;
  int ret;
  unsigned char c;
  int ErrCode;
  unsigned int time;

  ErrCode=init_packet(&pkt,fd_tty,fd_file,path);
  if(ErrCode < 0)
  	goto exit_ymodem_xfr;
  for(time = 0;;time++)
  {
    ret=read(pkt.fd_tty,&c,1);
	if(ret>0)
	  break;
	if(time>=TIME_TIMEOUT)
	{
	  ErrCode = ERR_CODE_NORSP;
	  goto exit_ymodem_xfr;
	}
  }
  if('C' == c)
  {
    pkt.head = HEAD;
    ErrCode = packet_xfr(&pkt);
	if(ErrCode < 0)
	  goto exit_ymodem_xfr;
    for(time = 0;;time++)
    {
      ret=read(pkt.fd_tty,&c,1);
	  if(ret>0)
	    break;
	  if(time>=TIME_TIMEOUT)
      {
	    ErrCode = ERR_CODE_NORSP;
	    goto exit_ymodem_xfr;
	  }
    }
    if(ACK == c)
    {
      pkt.head = DATA;
      ErrCode = packet_xfr(&pkt);
	  if(ErrCode < 0)
	    goto exit_ymodem_xfr;
    }
  }
exit_ymodem_xfr:
  free(pkt.file);
  free(pkt.buf);
  return ErrCode;
}

int open_port(char *sDev)
{
  int fd_tty;
  struct termios tio;
  fd_tty = open( sDev, O_RDWR|O_NOCTTY|O_NDELAY);
  if (fd_tty < 0)
  {
    printf("Can't Open Serial Port");
    return ERR_CODE_TTY;
  }
  if(fcntl(fd_tty, F_SETFL, 0)<0)
    printf("fcntl failed!\n");
  if(isatty(STDIN_FILENO)==0)
    printf("Stand input is not a terminal device!");

  bzero(&tio, sizeof(tio));
  tio.c_cflag |= CLOCAL | CREAD; 
  tio.c_cflag &= ~CSIZE; 
  tio.c_cflag |= CS8;
  tio.c_cflag &= ~PARENB;

  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);
    
  tio.c_cflag &=  ~CSTOPB;

  tio.c_cc[VTIME]  = 1;
  tio.c_cc[VMIN] = 0;
/*
  tio.c_oflag &=~(INLCR|IGNCR|ICRNL);
  tio.c_oflag &=~(ONLCR|OCRNL);
*/
  tcflush(fd_tty,TCIFLUSH);
  if((tcsetattr(fd_tty,TCSANOW,&tio))!=0)
  {
    printf("com set error");
    close(fd_tty);
    return ERR_CODE_TTY;
  }
  return fd_tty;
}

int enum_tty_occupied(char *stty)
{
  char msg_buff[2048]="lsof ",*ctemp;
  FILE *fp;

  if(!stty)
    return ERR_CODE_PARAM;

  strcat(msg_buff,stty);
  fp=popen(msg_buff,"r");
  if(fp)
  {
    ctemp=fgets(msg_buff,sizeof(msg_buff),fp);
    if(ctemp)
    {
      printf("===========================================\n");
      printf("The following Process open %s,may cause upgrade fail:\n%s",stty,msg_buff);
      do{
        ctemp=fgets(msg_buff,sizeof(msg_buff),fp);
        if(ctemp)
          printf("%s",msg_buff);
      }while(ctemp);
      printf("===========================================\n");
    }else
      printf("\nNo Process open %s\n",stty);
    pclose(fp);
  }else
  {
    printf("popen error");
    return ERR_CODE_POPEN;
  }
}

int main(int argc,char *argv[])
{
  int fd_tty,fd_file;
  int ErrCode = ERR_CODE_NONE;
  unsigned int i,j;
  unsigned char c;
  int ret;
  
  if(argc!=3)
  {
    printf("require 3 argument");
    ErrCode = ERR_CODE_PARAM;
    goto exit;
  }

  enum_tty_occupied(argv[1]);

  fd_tty=open_port(argv[1]);
  if(fd_tty<0)
  {
    printf("open tty port error");
    ErrCode = ERR_CODE_TTY;
    goto exit;
  }
  
  fd_file = open(argv[2],O_RDWR);
  if (fd_file < 0)
  {
    printf("open file error");
    ErrCode = ERR_CODE_FILE;
    goto exit;
  }
  
  printf("sync with mcu\n");

  for(i = 0;i < RETRY_CNT;i++)
  {
    system("echo 1 > /sys/class/leds/mcu_reset/brightness");
    usleep(100000);
    system("echo 0 > /sys/class/leds/mcu_reset/brightness");
    usleep(200000);
    c='C';
    write(fd_tty,&c,1);
    
    for(j = 0;j < 10*RETRY_CNT;j++)
    {
        ret = read(fd_tty,&c,1);
        if((ret>0)&&(c=='C'))
            break;
        usleep(100000);
    }
    if(j != RETRY_CNT)
      break;
  }
  if(i == RETRY_CNT)
  {
    printf("mcu no respond\n");
    ErrCode = ERR_CODE_NORSP;
    goto exit;
  }else
    printf("mcu bootloader online\n");

  ErrCode=ymodem_xfr(fd_tty,fd_file,argv[2]);

  switch(ErrCode)
  {
    case ERR_CODE_NONE:
      printf("Upgrade OK!!!\n");
      break;
    case ERR_CODE_PARAM:
      printf("ERR_CODE_PARAM!!!\n");
      break;
    case ERR_CODE_TTY:
      printf("ERR_CODE_TTY!!!\n");
      break;
    case ERR_CODE_GPIO:
      printf("ERR_CODE_GPIO!!!\n");
      break;
    case ERR_CODE_FILE:
      printf("ERR_CODE_FILE!!!\n");
      break;
    case ERR_CODE_NORSP:
      printf("ERR_CODE_NORSP!!!\n");
      break;
    case ERR_CODE_TTYTIMEOUT:
      printf("ERR_CODE_TTYTIMEOUT!!!\n");
      break;
    case ERR_CODE_MALLOC:
      printf("ERR_CODE_MALLOC!!!\n");
      break;
    case ERR_CODE_FILENAME:
      printf("ERR_CODE_FILENAME!!!\n");
      break;
    case ERR_CODE_POPEN:
      printf("ERR_CODE_POPEN!!!\n");
      break;
    case ERR_CODE_OTHERS:
      printf("ERR_CODE_OTHERS!!!\n");
      break;
    default:
      printf("ERR_CODE_UNKNOWN!!!\n");
      break;
  }
  
exit:
  printf("Reset mcu\n");
  system("echo 1 > /sys/class/leds/mcu_reset/brightness");
  usleep(100000);
  system("echo 0 > /sys/class/leds/mcu_reset/brightness");

  if(fd_tty > 0)
    close(fd_tty);
  if(fd_file > 0)
    close(fd_file);
  return ErrCode;
}


