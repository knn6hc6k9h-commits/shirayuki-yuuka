
static void load_callname(void){
  char path[1200];
  unsigned char buf[12+CALL_BASE_MAX+CALL_NAME_MAX+4];
  HANDLE f;DWORD got=0;unsigned long bl,cl;int i;
  if(g_call_loaded)return;
  g_call_loaded=1;g_call_enabled=0;g_call_base[0]=0;g_call_name[0]=0;g_pending_base[0]=0;

  build_path(path,(int)sizeof(path),g_dir,"yuuka_callname.dat");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;
  if(!ReadFile(f,buf,(DWORD)sizeof(buf),&got,0))got=0;
  CloseHandle(f);
  if(got<12||buf[0]!='Y'||buf[1]!='C'||buf[2]!='A'||buf[3]!='L')return;
  bl=u32(buf+4);cl=u32(buf+8);
  if(bl>CALL_BASE_MAX||cl>CALL_NAME_MAX||12ul+bl+cl>(unsigned long)got)return;
  for(i=0;i<(int)bl;++i)g_call_base[i]=(char)buf[12+i];g_call_base[bl]=0;
  for(i=0;i<(int)cl;++i)g_call_name[i]=(char)buf[12+bl+i];g_call_name[cl]=0;
  g_call_enabled=(bl>0&&cl>0);
}

static int save_callname_record(const char* base,const char* call){
  char path[1200];
  unsigned char buf[12+CALL_BASE_MAX+CALL_NAME_MAX+4];
  HANDLE f;DWORD wrote=0;int bl=slen(base),cl=slen(call),i,total;
  if(bl<0||bl>CALL_BASE_MAX||cl<0||cl>CALL_NAME_MAX)return 0;
  buf[0]='Y';buf[1]='C';buf[2]='A';buf[3]='L';p32(buf+4,(unsigned long)bl);p32(buf+8,(unsigned long)cl);
  for(i=0;i<bl;++i)buf[12+i]=(unsigned char)base[i];
  for(i=0;i<cl;++i)buf[12+bl+i]=(unsigned char)call[i];
  total=12+bl+cl;
  build_path(path,(int)sizeof(path),g_dir,"yuuka_callname.dat");
  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!WriteFile(f,buf,(DWORD)total,&wrote,0))wrote=0;
  CloseHandle(f);
  if(wrote!=(DWORD)total)return 0;

  for(i=0;i<bl;++i)g_call_base[i]=base[i];g_call_base[bl]=0;
  for(i=0;i<cl;++i)g_call_name[i]=call[i];g_call_name[cl]=0;
  g_call_loaded=1;g_call_enabled=(bl>0&&cl>0);
  return 1;
}

static int reset_callname_to_default(void){
  load_callname();
  if(g_call_base[0])return save_callname_record(g_call_base,"");
  return save_callname_record("","");
}

static int make_name_with_suffix(const char* base,const char* suffix,char* out,int cap){
  int p=0;
  out[0]=0;append_text(out,cap,&p,base);append_text(out,cap,&p,suffix);
  return p>0&&p<cap;
}

static HGLOBAL replace_all(HGLOBAL src,long* lenp,const char* needle,const char* repl){
  const char* s=(const char*)src;char* d;HGLOBAL out;
  long n=lenp?*lenp:0,i=0,w=0,newn;int nl=slen(needle),rl=slen(repl),count=0,j;
  if(!src||n<=0||nl<=0)return src;
  if(nl==rl){for(j=0;j<nl&&needle[j]==repl[j];++j){}if(j==nl)return src;}
  while(i+nl<=n){
    for(j=0;j<nl&&s[i+j]==needle[j];++j){}
    if(j==nl){++count;i+=nl;}else ++i;
  }
  if(!count)return src;
  newn=n+(long)count*(long)(rl-nl);
  out=GlobalAlloc(GPTR,(SIZE_T)newn+1);if(!out)return src;d=(char*)out;i=0;
  while(i<n){
    if(i+nl<=n){for(j=0;j<nl&&s[i+j]==needle[j];++j){}}
    else j=0;
    if(j==nl){copy_bytes(d+w,repl,rl);w+=rl;i+=nl;}
    else d[w++]=s[i++];
  }
  d[newn]=0;GlobalFree(src);if(lenp)*lenp=newn;return out;
}

static HGLOBAL apply_callname(HGLOBAL r,long* lenp){
  char target[CALL_BASE_MAX+16];int p=0;
  if(!r)return r;
  load_callname();if(!g_call_enabled)return r;
  target[0]=0;append_text(target,(int)sizeof(target),&p,g_call_base);append_text(target,(int)sizeof(target),&p,"さん");
  r=replace_all(r,lenp,target,g_call_name);
  /* Also fixes wrapper-generated text that still contains the literal token. */
  r=replace_all(r,lenp,"{name}さん",g_call_name);
  return r;
}

static HGLOBAL inject_callname_setting(HGLOBAL r,long* lenp){
  static const char anchor[]="\\q[今日のゆうかの状態,OnYuukaTodayCondition]\\n\\n";
  static const char replacement[]="\\q[今日のゆうかの状態,OnYuukaTodayCondition]\\n\\n\\q[呼び方を変更,OnYuukaCallNameSettings]\\n\\n";
  static const char fallback[]="\\q[その他の設定,OnYuukaOriginalSettings]";
  static const char fallback_repl[]="\\q[呼び方を変更,OnYuukaCallNameSettings]\\n\\q[その他の設定,OnYuukaOriginalSettings]";
  HGLOBAL before=r;
  r=replace_all(r,lenp,anchor,replacement);
  if(r==before)r=replace_all(r,lenp,fallback,fallback_repl);
  return r;
}
