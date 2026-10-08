#include <unistd.h>
extern int ChannelCreate(unsigned flags);
extern int MsgReceive(int chid, void *msg, unsigned long bytes, void *info);
extern int MsgReply(int rcvid, int status, const void *msg, unsigned long bytes);
extern int getpid(void);
static void ph(unsigned long v){ static const char h[]="0123456789abcdef"; char b[11]; int i;
  b[0]='0';b[1]='x'; for(i=0;i<8;i++)b[2+i]=h[(v>>((7-i)*4))&0xf]; b[10]=' '; write(1,b,11); }
int main(void){
    int chid;
    write(1,"S1\n",3);
    chid = ChannelCreate(0);
    write(1,"S2 chid=",8); ph(chid); write(1,"\n",1);
    write(1,"SERVER pid=",11); ph(getpid()); write(1,"\n",1);
    for(;;){
        char msg[64]; char rep[64];
        int rcvid = MsgReceive(chid, msg, sizeof msg, 0);
        write(1,"RECV rcvid=",11); ph(rcvid); write(1,"\n",1);
        if (rcvid < 0) continue;
        write(1,"SERVER got: ",12); write(1,msg,4); write(1,"\n",1);
        rep[0]='P'; rep[1]='O'; rep[2]='N'; rep[3]='G';
        MsgReply(rcvid, 0, rep, 4);
    }
}
