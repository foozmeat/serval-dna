/*
  Network simulator
*/

#include <signal.h>

#include "mem.h"
#include "socket.h"
#include "fdqueue.h"
#include "str.h"
#include "strbuf.h"
#include "strbuf_helpers.h"
#include "conf.h"
#include "net.h"

#define MTU 1600
struct peer;

struct packet{
  struct packet *_next;
  time_ms_t recv_time;
  struct peer *destination;
  size_t len;
  unsigned char buff[MTU];
};

struct peer{
  struct sched_ent alarm;
  struct peer *_next;
  struct network *network;
  struct socket_address addr;
  int packet_count;
  int max_packets;
  struct packet *_head, **_tail;
};

#define MAX_NETWORKS 20

struct network{
  struct sched_ent alarm;
  char path[256];
  long latency;
  long channel_sense_delay;
  long bit_rate;
  long error_rate;
  uint8_t echo;
};

struct profile_total broadcast_stats={
  .name="sock_alarm"
};
struct profile_total unicast_stats={
  .name="unicast_alarm"
};

struct peer *root=NULL;
struct network sockets[MAX_NETWORKS];
static int running;
void unicast_alarm(struct sched_ent *alarm);

const struct __sourceloc __whence = __NOWHERE__;

static const char *_trimbuildpath(const char *path)
{
  /* Remove common path prefix */
  int lastsep = 0;
  int i;
  for (i = 0; __FILE__[i] && path[i]; ++i) {
    if (i && path[i - 1] == '/')
      lastsep = i;
    if (__FILE__[i] != path[i])
      break;
  }
  return &path[lastsep];
}

void logMessage(int level, struct __sourceloc whence, const char *fmt, ...)
{
  const char *levelstr = "UNKWN:";
  switch (level) {
    case LOG_LEVEL_FATAL: levelstr = "FATAL:"; break;
    case LOG_LEVEL_ERROR: levelstr = "ERROR:"; break;
    case LOG_LEVEL_INFO:  levelstr = "INFO:"; break;
    case LOG_LEVEL_WARN:  levelstr = "WARN:"; break;
    case LOG_LEVEL_DEBUG: levelstr = "DEBUG:"; break;
  }
  fprintf(stderr, "%s ", levelstr);
  if (whence.file) {
    fprintf(stderr, "%s", _trimbuildpath(whence.file));
    if (whence.line)
      fprintf(stderr, ":%u", whence.line);
    if (whence.function)
      fprintf(stderr, ":%s()", whence.function);
    fputc(' ', stderr);
  } else if (whence.function) {
    fprintf(stderr, "%s() ", whence.function);
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

void logFlush()
{
}
void logConfigChanged()
{
}

int serverMode=0;

static void recv_packet(int fd, struct network *network, struct peer *destination)
{
  struct socket_address addr;
  struct packet *packet=emalloc_zero(sizeof(struct packet));
  if (!packet)
    return;
  packet->recv_time = gettime_ms();
  packet->destination = destination;
  struct iovec iov[]={
    {
      .iov_base = (void*)&packet->buff,
      .iov_len = sizeof(packet->buff)
    }
  };
  struct msghdr hdr={
    .msg_name=(void *)&addr.addr,
    .msg_namelen=sizeof(addr.store),
    .msg_iov=iov,
    .msg_iovlen=1,
  };
  ssize_t ret = recvmsg(fd, &hdr, 0);
  if (ret==-1){
    free(packet);
    WHYF_perror("recvmsg(%d,...)", fd);
    return;
  }
  addr.addrlen = hdr.msg_namelen;
  packet->len = ret;
  
  struct peer *peer = root;
  while(peer){
    if (peer->network == network
      && cmp_sockaddr(&addr, &peer->addr)==0)
      break;
    peer=peer->_next;
  }
  if (!peer){
    DEBUGF("New peer %s", alloca_socket_address(&addr));
    struct socket_address unicast_addr;
    unicast_addr.local.sun_family=AF_UNIX;
    strbuf d = strbuf_local(unicast_addr.local.sun_path, sizeof unicast_addr.local.sun_path);
    static unsigned peerid=0;
    strbuf_sprintf(d, "%s/peer%d", network->path, peerid++);
    if (strbuf_overrun(d)){
      WHY("Path too long");
      free(packet);
      return;
    }
    
    unicast_addr.addrlen=sizeof unicast_addr.local.sun_family + strlen(unicast_addr.local.sun_path) + 1;
    
    peer = emalloc_zero(sizeof(struct peer));
    if (!peer){
      free(packet);
      return;
    }
    peer->alarm.poll.fd=esocket(AF_UNIX, SOCK_DGRAM, 0);
    if (peer->alarm.poll.fd==-1){
      free(packet);
      free(peer);
      return;
    }
    if (socket_bind(peer->alarm.poll.fd, &unicast_addr.addr, unicast_addr.addrlen)==-1){
      free(packet);
      free(peer);
      return;
    }
    set_nonblock(peer->alarm.poll.fd);
    peer->alarm.function=unicast_alarm;
    peer->alarm.poll.events=POLLIN;
    peer->alarm.context = peer;
    peer->network = network;
    peer->addr = addr;
    peer->_next = root;
    peer->_tail = &peer->_head;
    peer->max_packets = 100;
    peer->alarm.stats=&unicast_stats;
    watch(&peer->alarm);
    root = peer;
  }
  
  if (peer->packet_count < peer->max_packets){
    *peer->_tail = packet;
    peer->_tail = &packet->_next;
    peer->packet_count++;
    if (!is_scheduled(&network->alarm)){
      network->alarm.alarm = packet->recv_time + network->latency;
      network->alarm.deadline = network->alarm.alarm;
      schedule(&network->alarm);
    }
  }else{
    // drop packets if the network queue is congested
    free(packet);
  }
}

void unicast_alarm(struct sched_ent *alarm)
{
  struct peer *peer = (struct peer*)alarm->context;
  
  if (alarm->poll.revents & POLLIN){
    recv_packet(alarm->poll.fd, peer->network, peer);
  }
}

void sock_alarm(struct sched_ent *alarm)
{
  struct network *network = (struct network*)alarm->context;
  
  if (alarm->poll.revents & POLLIN){
    recv_packet(alarm->poll.fd, network, NULL);
  }
  
  if (alarm->poll.revents == 0){
    // send a packet
    time_ms_t now = gettime_ms();
    struct peer *peer = root, *sender = NULL;
    struct packet *packet = NULL;
    alarm->alarm = TIME_NEVER_WILL;
    
    while(peer){
      if (peer->network == network && peer->_head){
	
	if (!packet && peer->_head->recv_time + network->latency <= now){
	  // TODO pick transmitter via simulated tdma scheme, ignore sender on collision
	  sender = peer;
	  packet = sender->_head;
	  sender->packet_count--;
	  sender->_head = packet->_next;
	  if (!sender->_head)
	    sender->_tail = &sender->_head;
	}
	
	// work out when the next packet should be sent
	if (peer->_head && peer->_head->recv_time + network->latency <= alarm->alarm)
	  alarm->alarm = peer->_head->recv_time + network->latency;
      }
      peer=peer->_next;
    }
    
    schedule(alarm);
    if (!sender)
      return;
      
    // deliver the packet
    struct iovec iov[]={
      {
	.iov_base = (void*)&packet->buff,
	.iov_len = packet->len
      }
    };
    struct msghdr hdr={
      .msg_iov=iov,
      .msg_iovlen=1,
    };
    
    peer = root;
    while(peer){
      if (peer->network == network
	&& (packet->destination == peer || !packet->destination)
	&& (network->echo || peer !=sender)){
	hdr.msg_name=(void *)&peer->addr.addr;
	hdr.msg_namelen=peer->addr.addrlen;
	// probably dont care if it fails..
	sendmsg(sender->alarm.poll.fd, &hdr, 0);
      }
      peer = peer->_next;
    }
    free(packet);
  }
}

void signal_handler(int UNUSED(signal))
{
  running=0;
}

int main(int argc, char **argv)
{
  int i;
  int index=0;
  long latency=0;
  long bit_rate=1024*1024;
  running = 0;
  uint8_t echo=1;
  bzero(&sockets, sizeof sockets);
  cf_init();
  
  for (i=1;i<argc;i++){
    if (*argv[i]=='-'){
      switch(argv[i][1]){
	case 'e':
	  echo=1;
	  break;
	case 'q':
	  echo=0;
	  break;
	case 'l':
	  latency = atol(argv[i]+2);
	  break;
	case 'b':
	  bit_rate = atol(argv[i]+2);
	default:
	case 'h':
	  running = -1;
	  break;
      }
      continue;
    }
    
    if (index>=MAX_NETWORKS){
      running = -1;
      fprintf(stderr, "Too many networks\n");
    }
    
    if (running == -1)
      break;
      
    struct network *sock = &sockets[index];
    
    snprintf(sock->path, sizeof(sock->path), "%s", argv[i]);
    
    struct socket_address addr;
    addr.local.sun_family=AF_UNIX;
    
    strbuf d = strbuf_local(addr.local.sun_path, sizeof addr.local.sun_path);
    strbuf_path_join(d, sock->path, "broadcast", NULL);
    if (strbuf_overrun(d))
      return WHY("Path too long");
    
    addr.addrlen=sizeof addr.local.sun_family + strlen(addr.local.sun_path) + 1;
    
    sock->alarm.poll.fd=esocket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock->alarm.poll.fd==-1)
      return 1;
    if (socket_bind(sock->alarm.poll.fd, &addr.addr, addr.addrlen)==-1)
      return 1;
    set_nonblock(sock->alarm.poll.fd);
    INFOF("Created socket %d for network %s", sock->alarm.poll.fd, alloca_socket_address(&addr));
    INFOF("Latency %ld, bitrate %ld", latency, bit_rate);
    sock->latency = latency;
    sock->bit_rate = bit_rate;
    sock->echo = echo;
    sock->alarm.function=sock_alarm;
    sock->alarm.poll.events=POLLIN;
    sock->alarm.stats=&broadcast_stats;
    sock->alarm.context=sock;
    watch(&sock->alarm);
    index++;
    running = 1;
  }
  
  /* Catch SIGHUP etc so that we can respond to requests to do things, eg, shut down. */
  struct sigaction sig;
  sig.sa_handler = signal_handler;
  sigemptyset(&sig.sa_mask);
  sigaddset(&sig.sa_mask, SIGHUP);
  sigaddset(&sig.sa_mask, SIGINT);
  sig.sa_flags = 0;
  sigaction(SIGHUP, &sig, NULL);
  sigaction(SIGINT, &sig, NULL);
  
  if (running==1)
    INFOF("Started");
  else
    fprintf(stderr, "Usage: %s [[-e] [-q] [-l<latency>] [-b<bitrate>] folder path] ...\n", argv[0]);
  
  while(running==1 && fd_poll())
    ;
  
  for (i=0; i<index; i++){
    INFOF("Closing network socket %d", sockets[i].alarm.poll.fd);
    unwatch(&sockets[i].alarm);
    socket_unlink_close(sockets[i].alarm.poll.fd);
  }
  
  struct peer *p = root;
  while(p){
    INFOF("Closing peer unicast socket %d", p->alarm.poll.fd);
    socket_unlink_close(p->alarm.poll.fd);
    p=p->_next;
  }
  
  return running;
}