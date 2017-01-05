#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "ymodem.h"
#include "serial.h"

#define RESEND 0
#define SENDED 1
#define PACKET_SIZE 128
//#define PACKET_SIZE 1024

enum STATE{
	START,
	RECEIVE_DATA,
	END,
	STOP
};

int ymodem_receive(const char *path, int fd) {
	int fd_file;
	unsigned char start_char, tmp_char;
	int state, len, i;
	int trans_end = 0;
	char packet_num; 
	int packet_size;
	int file_size = 0;
	char buf[1024] = {0};

	fd_file = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);	
	if (fd_file < 0)
		perror("open() file");
	
	packet_num = 0;
	packet_size = 0;
	state = START;
	while(1) {

		if (trans_end == 1) break;
		switch(state) {
			case START:
				printf("\033[32mSTART_MODE:\033[0m\n");
				put_char(fd, 'C');

				//start_char = get_char(fd);
				//printf("%x error \n", start_char);
				start_char = get_char(fd);
				if (start_char == SOH)
					printf("SOH: packet_128\n");
				else if (start_char == STX)
					printf("STX: packet_1024\n");
				else {
					printf("start_char %x error \n", start_char);
					//exit(1);
				}
				
				if (start_char == SOH || start_char == STX){
					tmp_char = get_char(fd);
					if (tmp_char != 0x00) 
						return -1; //not 00
					printf("0x00: %x\n", tmp_char);

					tmp_char = get_char(fd);
					if (tmp_char != 0xff) {
						printf("0xff: %x  error\n", tmp_char);
						return -1; //not ff
					}
					printf("0xff: %x\n", tmp_char);

					//receive data
					printf("sizeof buf is %d\n", sizeof(buf));
					for (i = 0; i < PACKET_SIZE; i++) {
						buf[i] = get_char(fd);
						//printf("i = %d ", i);
					}
					
				
					//get name and file size
					printf("receive file name: %s, length :%d\n",buf, strlen(buf));
					//printf("file size: %s\n", (buf + strlen(buf) + 1));
					file_size = atoi((buf + strlen(buf) + 1));			
					printf("file size: %d\n", file_size);
				}
	
				//crc 
				tmp_char = get_char(fd);
				printf("crc1 : %x\n", tmp_char);
				tmp_char = get_char(fd);
				printf("crc2 : %x\n", tmp_char);

				put_char(fd, ACK);
				packet_num = 1;
				put_char(fd, 'C');
					
				state = RECEIVE_DATA;
				break;
			case RECEIVE_DATA:
				printf("RECEIVE_DATA:\n");
				start_char = get_char(fd);
				switch(start_char){
					case EOT: 
						state = END;
						printf("\033[32mEOT\033[0m\n");
						put_char(fd, ACK); 
						break;
					case STX: 
						packet_size = 1024; 
						printf("STX ");
						break;
					case SOH: 
						packet_size = 128; 
						printf("SOH ");
						break;
					case CAN: break;
					default: break;
				}
				if (state == END) break;
				
				tmp_char = get_char(fd);
				printf("%x ", tmp_char);
				tmp_char = get_char(fd);
				printf("%x ", tmp_char);

				for(i = 0; i < packet_size; i++){
					buf[i] = get_char(fd);
					putchar(buf[i]);
				}
				
				tmp_char = get_char(fd);
				printf("%x ", tmp_char);
				tmp_char = get_char(fd);
				printf("%x -->\n", tmp_char);
				printf("i = %d\n", i);

				printf("packet_size : %d\n", packet_size);	
				printf("file_size   : %d\n", file_size);	
				if (file_size < packet_size) {
					len = write(fd_file, buf, file_size);
					file_size -= len;
				} else { 
					write(fd_file, buf, sizeof(buf));
					file_size -= sizeof(buf);
				}
				put_char(fd, ACK);

				//state = END;
				break;
			case END:
				printf("END_MODE:\n");
				put_char(fd, 'C');

				start_char = get_char(fd);
				switch(start_char){
					case STX: 
						packet_size = 1024; 
						printf("STX ");
						break;
					case SOH: 
						packet_size = 128; 
						printf("SOH ");
						break;
					case CAN: break;
					default: break;
				}
				//if (state == ) break;

				tmp_char = get_char(fd);
				printf("%x ", tmp_char);
				tmp_char = get_char(fd);
				printf("%x ", tmp_char);

				for(i = 0; i < packet_size; i++){
					buf[i] = get_char(fd);
					putchar(buf[i]);
				}

				tmp_char = get_char(fd);
				printf("%x ", tmp_char);
				tmp_char = get_char(fd);
				printf("%x -->\n", tmp_char);

				put_char(fd, ACK);
				state = STOP;
				break;
			case STOP:
				trans_end = 1;
				break;
		}
	}
	close(fd_file);

	return 0;
}

int open_port(void)
{
  int fd;
  struct termios tio;
  fd = open( "/dev/ttyS1", O_RDWR|O_NOCTTY|O_NDELAY);
  if (-1 == fd)
  {
    perror("Can't Open Serial Port");
    return(-1);
  }
  if(fcntl(fd, F_SETFL, 0)<0)
    printf("fcntl failed!\n");

	bzero(&tio, sizeof(tio));
  tio.c_cflag |= CLOCAL | CREAD; 
  tio.c_cflag &= ~CSIZE; 
	tio.c_cflag |= CS8;
  tio.c_cflag &= ~PARENB;

	cfsetispeed(&tio, B115200);
	cfsetospeed(&tio, B115200);
	
	tio.c_cflag &=  ~CSTOPB;

  tio.c_cc[VTIME]  = 0;
  tio.c_cc[VMIN] = 0;

	tio.c_oflag &=~(INLCR|IGNCR|ICRNL);
	tio.c_oflag &=~(ONLCR|OCRNL);

  tcflush(fd,TCIFLUSH);
  if((tcsetattr(fd,TCSANOW,&tio))!=0)
  {
    perror("com set error");
	  close(fd);
		return -1;
  }
  return fd;
}

int main(void)
{
  int fd,ret;
  char *path = "./mcu.hex";
  if((fd=open_port())<0)
  {
    perror("open_port error");
    return;
  }
  ret = ymodem_receive(path, fd);
	perror("----1----");
	if (ret < 0)
		printf("ymodem_receive() error\n");
  close(fd);
  return;
}


