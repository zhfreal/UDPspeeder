/*
 * comm.cpp
 *
 *  Created on: Jul 29, 2017
 *	  Author: wangyu
 */

#include "common.h"
#include "log.h"

#include <random>
#include <cmath>

int about_to_exit=0;

raw_mode_t raw_mode=mode_faketcp;
unordered_map<int, const char*> raw_mode_tostring = {{mode_faketcp, "faketcp"}, {mode_udp, "udp"}, {mode_icmp, "icmp"}};


//static int random_number_fd=-1;
char iptables_rule[200]="";
//int is_client = 0, is_server = 0;

program_mode_t client_or_server=unset_mode;//0 unset; 1client 2server

working_mode_t working_mode=tunnel_mode;

int socket_buf_size=1024*1024;

int init_ws()
{
#if defined(__MINGW32__)
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL.								  */
		printf("WSAStartup failed with error: %d\n", err);
		exit(-1);
	}

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater	*/
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we	  */
	/* requested.										*/

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.								  */
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		exit(-1);
	}
	else
	{
		printf("The Winsock 2.2 dll was found okay");
	}
	
	int tmp[]={0,100,200,300,500,800,1000,2000,3000,4000,-1};
	int succ=0;
	for(int i=1;tmp[i]!=-1;i++)
	{
		if(_setmaxstdio(100)==-1) break;
		else succ=i;
	}	
	printf(", _setmaxstdio() was set to %d\n",tmp[succ]);
#endif
return 0;
}

#if defined(__MINGW32__)
char *get_sock_error()
{
	static char buf[1000];
	int e=WSAGetLastError();
	wchar_t *s = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
			NULL, e,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&s, 0, NULL);
	sprintf(buf, "%d:%S", e,s);
	int len=strlen(buf);
	if(len>0&&buf[len-1]=='\n') buf[len-1]=0; 
	LocalFree(s);
	return buf;
}
int get_sock_errno()
{
	return WSAGetLastError();
}
#else
char *get_sock_error()
{
	static char buf[1000];
	sprintf(buf, "%d:%s", errno,strerror(errno));
	return buf;
}
int get_sock_errno()
{
	return errno;
}
#endif


struct my_random_t
{
	std::random_device rd;
	std::mt19937 gen;
	std::uniform_int_distribution<u64_t> dis64;
	std::uniform_int_distribution<u32_t> dis32;

	std::uniform_int_distribution<unsigned char> dis8;

	my_random_t()
	{
		std::mt19937 gen_tmp(rd());
		gen=gen_tmp;
		gen.discard(700000);  //magic
	}
	u64_t gen64()
	{
		return dis64(gen);
	}
	u32_t gen32()
	{
		return dis32(gen);
	}

	unsigned char gen8()
	{
		return dis8(gen);
	}
	/*int random_number_fd;
	random_fd_t()
	{
			random_number_fd=open("/dev/urandom",O_RDONLY);

			if(random_number_fd==-1)
			{
				mylog(log_fatal,"error open /dev/urandom\n");
				myexit(-1);
			}
			setnonblocking(random_number_fd);
	}
	int get_fd()
	{
		return random_number_fd;
	}*/
}my_random;

void get_fake_random_chars(char * s,int len)
{
	char *p=s;
	int left=len;

	while(left>=(int)sizeof(u64_t))
	{
		//*((u64_t*)p)=my_random.gen64();  //this may break strict-alias  ,  also p may not point to a multiple of sizeof(u64_t)

		u64_t tmp=my_random.gen64();
		memcpy(p,&tmp,sizeof(u64_t));  // so,use memcpy instead.

		p+=sizeof(u64_t);
		left-=sizeof(u64_t);
	}
	if(left)
	{
		u64_t tmp=my_random.gen64();
		memcpy(p,&tmp,left);
	}
}

int random_between(u32_t a,u32_t b)
{
	if(a>b)
	{
		mylog(log_fatal,"min >max?? %d %d\n",a ,b);
		myexit(1);
	}
	if(a==b)return a;
	else return a+get_fake_random_number()%(b+1-a);
}

/*
u64_t get_current_time()//ms
{
	timespec tmp_time;
	clock_gettime(CLOCK_MONOTONIC, &tmp_time);
	return ((u64_t)tmp_time.tv_sec)*1000llu+((u64_t)tmp_time.tv_nsec)/(1000*1000llu);
}

u64_t get_current_time_us()
{
	timespec tmp_time;
	clock_gettime(CLOCK_MONOTONIC, &tmp_time);
	return (uint64_t(tmp_time.tv_sec))*1000llu*1000llu+ (uint64_t(tmp_time.tv_nsec))/1000llu;
}*/

u64_t get_current_time()//ms
{
	//timespec tmp_time;
	//clock_gettime(CLOCK_MONOTONIC, &tmp_time);
	//return ((u64_t)tmp_time.tv_sec)*1000llu+((u64_t)tmp_time.tv_nsec)/(1000*1000llu);
	return (u64_t)(ev_time()*1000);
}

u64_t get_current_time_rough()//ms
{
	return (u64_t)(ev_now(ev_default_loop(0))*1000);
}

u64_t get_current_time_us()
{
	//timespec tmp_time;
	//clock_gettime(CLOCK_MONOTONIC, &tmp_time);
	//return (uint64_t(tmp_time.tv_sec))*1000llu*1000llu+ (uint64_t(tmp_time.tv_nsec))/1000llu;
	return (u64_t)(ev_time()*1000*1000);
}

u64_t pack_u64(u32_t a,u32_t b)
{
	u64_t ret=a;
	ret<<=32u;
	ret+=b;
	return ret;
}
u32_t get_u64_h(u64_t a)
{
	return a>>32u;
}
u32_t get_u64_l(u64_t a)
{
	return (a<<32u)>>32u;
}

void write_u16(char * p,u16_t w)
{
	*(unsigned char*)(p + 1) = (w & 0xff);
	*(unsigned char*)(p + 0) = (w >> 8);
}
u16_t read_u16(char * p)
{
	u16_t res;
	res = *(const unsigned char*)(p + 0);
	res = *(const unsigned char*)(p + 1) + (res << 8);
	return res;
}

void write_u32(char * p,u32_t l)
{
	*(unsigned char*)(p + 3) = (unsigned char)((l >>  0) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 0) = (unsigned char)((l >> 24) & 0xff);
}
u32_t read_u32(char * p)
{
	u32_t res;
	res = *(const unsigned char*)(p + 0);
	res = *(const unsigned char*)(p + 1) + (res << 8);
	res = *(const unsigned char*)(p + 2) + (res << 8);
	res = *(const unsigned char*)(p + 3) + (res << 8);
	return res;
}

void write_u64(char * s,u64_t a)
{
	assert(0==1);
}
u64_t read_u64(char * s)
{
	assert(0==1);
	return 0;
}


char * my_ntoa(u32_t ip)
{
	in_addr a;
	a.s_addr=ip;
	return inet_ntoa(a);
}


int add_iptables_rule(char * s)
{
	strcpy(iptables_rule,s);
	char buf[300]="iptables -I ";
	strcat(buf,s);
	if(system(buf)==0)
	{
		mylog(log_warn,"auto added iptables rule by:  %s\n",buf);
	}
	else
	{
		mylog(log_fatal,"auto added iptables failed by: %s\n",buf);
		myexit(-1);
	}
	return 0;
}

int clear_iptables_rule()
{
	if(iptables_rule[0]!=0)
	{
		char buf[300]="iptables -D ";
		strcat(buf,iptables_rule);
		if(system(buf)==0)
		{
			mylog(log_warn,"iptables rule cleared by: %s \n",buf);
		}
		else
		{
			mylog(log_error,"clear iptables failed by: %s\n",buf);
		}

	}
	return 0;
}



u64_t get_fake_random_number_64()
{
	//u64_t ret;
	//int size=read(random_fd.get_fd(),&ret,sizeof(ret));
	//if(size!=sizeof(ret))
	//{
	//	mylog(log_fatal,"get random number failed %d\n",size);

	//	myexit(-1);
	//}

	return my_random.gen64();
}
u32_t get_fake_random_number()
{
	//u32_t ret;
	//int size=read(random_fd.get_fd(),&ret,sizeof(ret));
	//if(size!=sizeof(ret))
	//{
	//	mylog(log_fatal,"get random number failed %d\n",size);
	//	myexit(-1);
	//}
	return my_random.gen32();
}
u32_t get_fake_random_number_nz() //nz for non-zero
{
	u32_t ret=0;
	while(ret==0)
	{
		ret=get_fake_random_number();
	}
	return ret;
}

/*
u64_t ntoh64(u64_t a)
{
	if(__BYTE_ORDER == __LITTLE_ENDIAN)
	{
		return __bswap_64( a);
	}
	else return a;

}
u64_t hton64(u64_t a)
{
	if(__BYTE_ORDER == __LITTLE_ENDIAN)
	{
		return __bswap_64( a);
	}
	else return a;

}*/

void setnonblocking(int sock) {
#if !defined(__MINGW32__)
	int opts;
	opts = fcntl(sock, F_GETFL);

	if (opts < 0) {
		mylog(log_fatal,"fcntl(sock,GETFL)\n");
		//perror("fcntl(sock,GETFL)");
		myexit(1);
	}
	opts = opts | O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) < 0) {
		mylog(log_fatal,"fcntl(sock,SETFL,opts)\n");
		//perror("fcntl(sock,SETFL,opts)");
		myexit(1);
	}
#else
	int iResult;
	u_long iMode = 1;
	iResult = ioctlsocket(sock, FIONBIO, &iMode);
	if (iResult != NO_ERROR)
		printf("ioctlsocket failed with error: %d\n", iResult);

#endif
}

/*
	Generic checksum calculation function
*/
unsigned short csum(const unsigned short *ptr,int nbytes) {
	long sum;
	unsigned short oddbyte;
	short answer;

	sum=0;
	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((u_char*)&oddbyte)=*(u_char*)ptr;
		sum+=oddbyte;
	}

	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	answer=(short)~sum;

	return(answer);
}


unsigned short tcp_csum(const pseudo_header & ph,const unsigned short *ptr,int nbytes) {//works both for big and little endian

	long sum;
	unsigned short oddbyte;
	short answer;

	sum=0;
	unsigned short * tmp= (unsigned short *)&ph;
	for(int i=0;i<6;i++)
	{
		sum+=*tmp++;
	}


	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((u_char*)&oddbyte)=*(u_char*)ptr;
		sum+=oddbyte;
	}

	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	answer=(short)~sum;

	return(answer);
}

int set_buf_size(int fd,int socket_buf_size,int force_socket_buf)
{
	if(0)
	{
/*
		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUFFORCE, &socket_buf_size, sizeof(socket_buf_size))<0)
		{
			mylog(log_fatal,"SO_SNDBUFFORCE fail  socket_buf_size=%d  errno=%s\n",socket_buf_size,strerror(errno));
			myexit(1);
		}
		if(setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &socket_buf_size, sizeof(socket_buf_size))<0)
		{
			mylog(log_fatal,"SO_RCVBUFFORCE fail  socket_buf_size=%d  errno=%s\n",socket_buf_size,strerror(errno));
			myexit(1);
		}
*/
	}
	else
	{
		if(setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &socket_buf_size, sizeof(socket_buf_size))<0)
		{
			mylog(log_fatal,"SO_SNDBUF fail  socket_buf_size=%d  errno=%s\n",socket_buf_size,get_sock_error());
			myexit(1);
		}
		if(setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &socket_buf_size, sizeof(socket_buf_size))<0)
		{
			mylog(log_fatal,"SO_RCVBUF fail  socket_buf_size=%d  errno=%s\n",socket_buf_size,get_sock_error());
			myexit(1);
		}
	}
	return 0;
}

void myexit(int a)
{
	if(enable_log_color)
   	 printf("%s\n",RESET);
   // clear_iptables_rule();
	exit(a);
}
void  signal_handler(int sig)
{
	about_to_exit=1;
	// myexit(0);
}
/*
int numbers_to_char(id_t id1,id_t id2,id_t id3,char * &data,int &len)
{
	static char buf[buf_len];
	data=buf;
	id_t tmp=htonl(id1);
	memcpy(buf,&tmp,sizeof(tmp));

	tmp=htonl(id2);
	memcpy(buf+sizeof(tmp),&tmp,sizeof(tmp));

	tmp=htonl(id3);
	memcpy(buf+sizeof(tmp)*2,&tmp,sizeof(tmp));

	len=sizeof(id_t)*3;
	return 0;
}


int char_to_numbers(const char * data,int len,id_t &id1,id_t &id2,id_t &id3)
{
	if(len<int(sizeof(id_t)*3)) return -1;
	id1=ntohl(  *((id_t*)(data+0)) );
	id2=ntohl(  *((id_t*)(data+sizeof(id_t))) );
	id3=ntohl(  *((id_t*)(data+sizeof(id_t)*2)) );
	return 0;
}
*/
bool larger_than_u32(u32_t a,u32_t b)
{

	u32_t smaller,bigger;
	smaller=min(a,b);//smaller in normal sense
	bigger=max(a,b);
	u32_t distance=min(bigger-smaller,smaller+(0xffffffff-bigger+1));
	if(distance==bigger-smaller)
	{
		if(bigger==a)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(smaller==b)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
}

bool larger_than_u16(uint16_t a,uint16_t b)
{

	uint16_t smaller,bigger;
	smaller=min(a,b);//smaller in normal sense
	bigger=max(a,b);
	uint16_t distance=min(bigger-smaller,smaller+(0xffff-bigger+1));
	if(distance==bigger-smaller)
	{
		if(bigger==a)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(smaller==b)
		{
			return 0;
		}
		else
		{
			return 1;
		}
	}
}




/*
int set_timer_ms(int epollfd,int &timer_fd,u32_t timer_interval)
{
	int ret;
	epoll_event ev;

	itimerspec its;
	memset(&its,0,sizeof(its));

	if((timer_fd=timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK)) < 0)
	{
		mylog(log_fatal,"timer_fd create error\n");
		myexit(1);
	}
	its.it_interval.tv_sec=(timer_interval/1000);
	its.it_interval.tv_nsec=(timer_interval%1000)*1000ll*1000ll;
	its.it_value.tv_nsec=1; //imidiately
	timerfd_settime(timer_fd,0,&its,0);


	ev.events = EPOLLIN;
	ev.data.fd = timer_fd;

	ret=epoll_ctl(epollfd, EPOLL_CTL_ADD, timer_fd, &ev);
	if (ret < 0) {
		mylog(log_fatal,"epoll_ctl return %d\n", ret);
		myexit(-1);
	}
	return 0;
}*/
/*
int create_new_udp(int &new_udp_fd,int remote_address_uint32,int remote_port)
{
	struct sockaddr_in remote_addr_in;

	socklen_t slen = sizeof(sockaddr_in);
	memset(&remote_addr_in, 0, sizeof(remote_addr_in));
	remote_addr_in.sin_family = AF_INET;
	remote_addr_in.sin_port = htons(remote_port);
	remote_addr_in.sin_addr.s_addr = remote_address_uint32;

	new_udp_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (new_udp_fd < 0) {
		mylog(log_warn, "create udp_fd error\n");
		return -1;
	}
	setnonblocking(new_udp_fd);
	set_buf_size(new_udp_fd);

	mylog(log_debug, "created new udp_fd %d\n", new_udp_fd);
	int ret = connect(new_udp_fd, (struct sockaddr *) &remote_addr_in, slen);
	if (ret != 0) {
		mylog(log_warn, "udp fd connect fail %d %s\n",ret,strerror(errno));
		close(new_udp_fd);
		return -1;
	}
	return 0;
}*/
void ip_port_t::from_u64(u64_t u64)
{
	ip=get_u64_h(u64);
	port=get_u64_l(u64);
}
u64_t ip_port_t::to_u64()
{
	return pack_u64(ip,port);
}
char * ip_port_t::to_s()
{
	static char res[40];
	sprintf(res,"%s:%d",my_ntoa(ip),port);
	return res;
}

int round_up_div(int a,int b)
{
	return (a+b-1)/b;
}

int create_fifo(char * file)
{
#if !defined(__MINGW32__)
	if(mkfifo (file, 0666)!=0)
	{
		if(errno==EEXIST)
		{
			mylog(log_warn,"warning fifo file %s exist\n",file);
		}
		else
		{
			mylog(log_fatal,"create fifo file %s failed\n",file);
			myexit(-1);
		}
	}
	int fifo_fd=open (file, O_RDWR);
	if(fifo_fd<0)
	{
		mylog(log_fatal,"create fifo file %s failed\n",file);
		myexit(-1);
	}
	struct stat st;
	if (fstat(fifo_fd, &st)!=0)
	{
		mylog(log_fatal,"fstat failed for fifo file %s\n",file);
		myexit(-1);
	}

	if(!S_ISFIFO(st.st_mode))
	{
		mylog(log_fatal,"%s is not a fifo\n",file);
		myexit(-1);
	}

	setnonblocking(fifo_fd);
	return fifo_fd;
#else
	assert(0==1&&"not supported\n");
	return 0;
#endif
}


int new_listen_socket(int &fd,u32_t ip,int port)
{
	fd =socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	int yes = 1;
	//setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	struct sockaddr_in local_me={0};

	socklen_t slen = sizeof(sockaddr_in);
	//memset(&local_me, 0, sizeof(local_me));
	local_me.sin_family = AF_INET;
	local_me.sin_port = htons(port);
	local_me.sin_addr.s_addr = ip;

	if (::bind(fd, (struct sockaddr*) &local_me, slen) == -1) {
		mylog(log_fatal,"socket bind error\n");
		//perror("socket bind error");
		myexit(1);
	}
	setnonblocking(fd);
	set_buf_size(fd,socket_buf_size);

	mylog(log_debug,"local_listen_fd=%d\n",fd);

	return 0;
}
int new_connected_socket(int &fd,u32_t ip,int port)
{
	char ip_port[40];
	sprintf(ip_port,"%s:%d",my_ntoa(ip),port);

	struct sockaddr_in remote_addr_in = { 0 };

	socklen_t slen = sizeof(sockaddr_in);
	//memset(&remote_addr_in, 0, sizeof(remote_addr_in));
	remote_addr_in.sin_family = AF_INET;
	remote_addr_in.sin_port = htons(port);
	remote_addr_in.sin_addr.s_addr = ip;

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd < 0) {
		mylog(log_warn, "[%s]create udp_fd error\n", ip_port);
		return -1;
	}
	setnonblocking(fd);
	set_buf_size(fd, socket_buf_size);

	mylog(log_debug, "[%s]created new udp_fd %d\n", ip_port, fd);
	int ret = connect(fd, (struct sockaddr *) &remote_addr_in, slen);
	if (ret != 0) {
		mylog(log_warn, "[%s]fd connect fail\n",ip_port);
		sock_close(fd);
		return -1;
	}
	return 0;
}
vector<string> parse_conf_line(const string& s0)
{
	string s=s0;
	s.reserve(s.length()+200);
	char *buf=(char *)s.c_str();
	//char buf[s.length()+200];
	char *p=buf;
	int i=int(s.length())-1;
	int j;
	vector<string>res;
	strcpy(buf,(char *)s.c_str());
	while(i>=0)
	{
		if(buf[i]==' ' || buf[i]== '\t')
			buf[i]=0;
		else break;
		i--;
	}
	while(*p!=0)
	{
		if(*p==' ' || *p== '\t')
		{
			p++;
		}
		else break;
	}
	int new_len=strlen(p);
	if(new_len==0)return res;
	if(p[0]=='#') return res;
	if(p[0]!='-')
	{
		mylog(log_fatal,"line :<%s> not begin with '-' ",s.c_str());
		myexit(-1);
	}

	for(i=0;i<new_len;i++)
	{
		if(p[i]==' '||p[i]=='\t')
		{
			break;
		}
	}
	if(i==new_len)
	{
		res.push_back(p);
		return res;
	}

	j=i;
	while(p[j]==' '||p[j]=='\t')
		j++;
	p[i]=0;
	res.push_back(p);
	res.push_back(p+j);
	return res;
}
