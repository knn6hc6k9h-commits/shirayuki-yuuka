typedef void* HGLOBAL;
typedef void* HMODULE;
typedef void* HANDLE;
typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned long SIZE_T;
typedef unsigned short WORD;
#define TRUE 1
#define GPTR 0x0040u
#define GENERIC_READ 0x80000000ul
#define GENERIC_WRITE 0x40000000ul
#define FILE_SHARE_READ 1ul
#define OPEN_EXISTING 3ul
#define CREATE_ALWAYS 2ul
#define FILE_ATTRIBUTE_NORMAL 0x80ul
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))

typedef struct _SYSTEMTIME {
  WORD wYear,wMonth,wDayOfWeek,wDay,wHour,wMinute,wSecond,wMilliseconds;
} SYSTEMTIME;

__declspec(dllimport) HGLOBAL __stdcall GlobalAlloc(unsigned int, SIZE_T);
__declspec(dllimport) HGLOBAL __stdcall GlobalFree(HGLOBAL);
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HMODULE,const char*);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);
__declspec(dllimport) void __stdcall GetLocalTime(SYSTEMTIME*);

typedef BOOL (__cdecl *load_fn)(HGLOBAL,long);
typedef BOOL (__cdecl *unload_fn)(void);
typedef HGLOBAL (__cdecl *request_fn)(HGLOBAL,long*);

static HMODULE g_mod=0;
static load_fn g_load=0;
static unload_fn g_unload=0;
static request_fn g_request=0;
static char g_dir[1024];
static unsigned char g_feature[64];
static int g_has_last_seen=0;

static void copy_bytes(char* d,const char* s,int n){int i;for(i=0;i<n;++i)d[i]=s[i];}
static int slen(const char* s){int n=0;while(s&&s[n])++n;return n;}
static unsigned long u32(const unsigned char* p){return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);}
static void p32(unsigned char* p,unsigned long v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}

static void build_path(char* out,int cap,const char* dir,const char* file){
  int n=0,i=0;out[0]=0;
  while(dir&&dir[i]&&n<cap-1)out[n++]=dir[i++];
  if(n>0&&out[n-1]!='\\'&&out[n-1]!='/'){if(n<cap-1)out[n++]='\\';}
  i=0;while(file&&file[i]&&n<cap-1)out[n++]=file[i++];out[n]=0;
}
static void build_core_path(char* out,int cap,const char* dir){build_path(out,cap,dir,"shiori_core.dll");}

static void init_feature(void){int i;for(i=0;i<64;++i)g_feature[i]=0;g_feature[0]='Y';g_feature[1]='U';g_feature[2]='K';g_feature[3]='F';p32(g_feature+4,5);g_has_last_seen=0;}
static void load_feature(void){
  char path[1200];HANDLE f;DWORD got=0;int i;
  init_feature();build_path(path,(int)sizeof(path),g_dir,"yuuka_features.dat");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;
  for(i=0;i<64;++i)g_feature[i]=0;
  if(!ReadFile(f,g_feature,64,&got,0))got=0;CloseHandle(f);
  if(got<28||g_feature[0]!='Y'||g_feature[1]!='U'||g_feature[2]!='K'||g_feature[3]!='F'){init_feature();return;}
  if(!u32(g_feature+4))p32(g_feature+4,1);
  g_has_last_seen=(u32(g_feature+8)>=2000&&u32(g_feature+12)>=1&&u32(g_feature+12)<=12&&u32(g_feature+16)>=1&&u32(g_feature+16)<=31);
}
static void save_feature_data(void){
  char path[1200];HANDLE f;DWORD wrote=0;
  p32(g_feature+4,5);build_path(path,(int)sizeof(path),g_dir,"yuuka_features.dat");
  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;WriteFile(f,g_feature,64,&wrote,0);CloseHandle(f);
}
static void save_last_seen(void){
  SYSTEMTIME s;
  GetLocalTime(&s);p32(g_feature+8,s.wYear);p32(g_feature+12,s.wMonth);p32(g_feature+16,s.wDay);p32(g_feature+20,s.wHour);p32(g_feature+24,s.wMinute);p32(g_feature+28,s.wSecond);g_has_last_seen=1;
  save_feature_data();
}
static int leap(unsigned long y){return (y%4==0&&y%100!=0)||y%400==0;}
static unsigned long daystamp(unsigned long y,unsigned long m,unsigned long d){
  static const unsigned char md[12]={31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned long x=0,yy,mm;if(y<2000||m<1||m>12||d<1||d>31)return 0;
  for(yy=2000;yy<y;++yy)x+=leap(yy)?366:365;
  for(mm=1;mm<m;++mm){x+=md[mm-1];if(mm==2&&leap(y))++x;}
  return x+d-1;
}
static unsigned long away_seconds(void){
  SYSTEMTIME s;unsigned long a,b,prev,cur,sec=0;
  if(!g_has_last_seen)return 0;GetLocalTime(&s);
  a=daystamp(u32(g_feature+8),u32(g_feature+12),u32(g_feature+16));b=daystamp(s.wYear,s.wMonth,s.wDay);
  if(!a||!b||b<a)return 0;
  if(u32(g_feature+4)>=2&&u32(g_feature+28)<=59)sec=u32(g_feature+28);
  prev=u32(g_feature+20)*3600ul+u32(g_feature+24)*60ul+sec;
  cur=(unsigned long)s.wHour*3600ul+(unsigned long)s.wMinute*60ul+s.wSecond;
  if(a==b&&cur<prev)return 0;return (b-a)*86400ul+cur-prev;
}
static unsigned long today_key(void){SYSTEMTIME s;GetLocalTime(&s);return (unsigned long)s.wYear*10000ul+(unsigned long)s.wMonth*100ul+s.wDay;}
static unsigned long pick_daily_state(void){
  SYSTEMTIME s;unsigned long x;GetLocalTime(&s);
  x=(unsigned long)s.wYear*10000ul+(unsigned long)s.wMonth*100ul+s.wDay;
  x^=((unsigned long)s.wHour*3600ul+(unsigned long)s.wMinute*60ul+s.wSecond)*1103515245ul+12345ul;
  x^=(x>>16);x*=2654435761ul;x^=(x>>13);return x%5ul;
}
static void ensure_daily_state(void){
  unsigned long key=today_key(),state=u32(g_feature+52);
  if(u32(g_feature+48)!=key||state>4ul){p32(g_feature+48,key);p32(g_feature+52,pick_daily_state());p32(g_feature+56,0);save_feature_data();}
}
static const char* daily_state_event(unsigned long state){
  if(state==1ul)return "OnYuukaFeatureSleepy";
  if(state==2ul)return "OnYuukaFeatureEnergetic";
  if(state==3ul)return "OnYuukaFeatureHungry";
  if(state==4ul)return "OnYuukaFeatureReading";
  return 0;
}

static int request_has_id(const char* p,int n,const char* id){
  int i,j,k=slen(id);
  if(!p||n<=0)return 0;
  for(i=0;i+4+k<=n;++i){
    if((i==0||p[i-1]=='\n')&&p[i]=='I'&&p[i+1]=='D'&&p[i+2]==':'&&p[i+3]==' '){
      for(j=0;j<k&&i+4+j<n&&p[i+4+j]==id[j];++j){}
      if(j==k&&(i+4+j==n||p[i+4+j]=='\r'||p[i+4+j]=='\n'))return 1;
    }
  }
  return 0;
}
static int get_header_value(const char* p,int n,const char* key,char* out,int cap){
  int i,j,k=slen(key),w=0;if(!p||!out||cap<1)return 0;out[0]=0;
  for(i=0;i+k<=n;++i){
    if((i==0||p[i-1]=='\n')){
      for(j=0;j<k&&i+j<n&&p[i+j]==key[j];++j){}
      if(j==k){i+=k;if(i<n&&p[i]==' ')++i;while(i<n&&p[i]!='\r'&&p[i]!='\n'&&w<cap-1)out[w++]=p[i++];out[w]=0;return 1;}
    }
  }
  return 0;
}

static HGLOBAL make_response(const char* value,long* outlen){
  static const char a[]="SHIORI/3.0 200 OK\r\nCharset: UTF-8\r\nSender: ShirayukiYuuka\r\nValue: ";
  static const char e[]="\r\n\r\n";
  int na=(int)(sizeof(a)-1),nv=slen(value),ne=(int)(sizeof(e)-1),n=na+nv+ne;char* p=(char*)GlobalAlloc(GPTR,(SIZE_T)n+1);
  if(!p){if(outlen)*outlen=0;return 0;}copy_bytes(p,a,na);copy_bytes(p+na,value,nv);copy_bytes(p+na+nv,e,ne);p[n]=0;if(outlen)*outlen=n;return p;
}

static HGLOBAL core_event(const char* id,long* outlen){
  static const char a[]="GET SHIORI/3.0\r\nCharset: UTF-8\r\nSender: SSP\r\nSecurityLevel: local\r\nID: ";
  static const char e[]="\r\n\r\n";
  int na=(int)(sizeof(a)-1),ni=slen(id),ne=(int)(sizeof(e)-1),n=na+ni+ne;HGLOBAL h;HGLOBAL r;long rn=n;
  h=GlobalAlloc(0,(SIZE_T)n);if(!h||!g_request){if(outlen)*outlen=0;return 0;}
  copy_bytes((char*)h,a,na);copy_bytes((char*)h+na,id,ni);copy_bytes((char*)h+na+ni,e,ne);
  r=g_request(h,&rn);if(outlen)*outlen=rn;return r;
}

static int last_number(const char* p,long n){
  long i=0;int found=-1;
  while(p&&i<n){
    if(p[i]>='0'&&p[i]<='9'){
      int v=0;while(i<n&&p[i]>='0'&&p[i]<='9'){v=v*10+(p[i]-'0');++i;}found=v;
    }else ++i;
  }
  return found;
}
static int query_affection(void){
  long rn=0;HGLOBAL r=core_event("OnYuukaAffection",&rn);int v=-1;
  if(r){v=last_number((const char*)r,rn);GlobalFree(r);}return v;
}

static int valid_birthday(int m,int d){
  static const unsigned char md[12]={31,29,31,30,31,30,31,31,30,31,30,31};
  return m>=1&&m<=12&&d>=1&&d<=md[m-1];
}
static int parse_birthday(const char* s,int* month,int* day){
  int i=0,m=0,d=0,have=0;if(!s)return 0;
  while(s[i]==' '||s[i]=='\t')++i;
  while(s[i]>='0'&&s[i]<='9'){m=m*10+(s[i]-'0');++i;have=1;}if(!have)return 0;
  while(s[i]==' '||s[i]=='\t')++i;
  if(s[i]!='/'&&s[i]!='-'&&s[i]!='.')return 0;++i;
  while(s[i]==' '||s[i]=='\t')++i;have=0;
  while(s[i]>='0'&&s[i]<='9'){d=d*10+(s[i]-'0');++i;have=1;}if(!have)return 0;
  while(s[i]==' '||s[i]=='\t')++i;if(s[i])return 0;
  if(!valid_birthday(m,d))return 0;*month=m;*day=d;return 1;
}
static void append_text(char* out,int cap,int* pos,const char* s){int i=0;while(s&&s[i]&&*pos<cap-1)out[(*pos)++]=s[i++];out[*pos]=0;}
static void append_uint(char* out,int cap,int* pos,unsigned long v){char t[16];int n=0,i;if(v==0)t[n++]='0';else while(v&&n<15){t[n++]=(char)('0'+v%10);v/=10;}for(i=n-1;i>=0;--i)if(*pos<cap-1)out[(*pos)++]=t[i];out[*pos]=0;}
static HGLOBAL birthday_settings(long* outlen){
  char b[1200];int p=0;unsigned long m=u32(g_feature+36),d=u32(g_feature+40);b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]設定ですね♪\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[今日のゆうかの状態,OnYuukaTodayCondition]\\n\\n");
  if(valid_birthday((int)m,(int)d)){
    append_text(b,sizeof(b),&p,"登録されている誕生日：");append_uint(b,sizeof(b),&p,m);append_text(b,sizeof(b),&p,"月");append_uint(b,sizeof(b),&p,d);append_text(b,sizeof(b),&p,"日\\n\\n");
    append_text(b,sizeof(b),&p,"\\q[誕生日を変更,OnYuukaFeatureBirthdayPrompt]\\n\\q[誕生日の登録を消す,OnYuukaBirthdayClear]\\n");
  }else append_text(b,sizeof(b),&p,"誕生日はまだ登録されていません。\\n\\n\\q[誕生日を登録,OnYuukaFeatureBirthdayPrompt]\\n");
  append_text(b,sizeof(b),&p,"\\q[その他の設定,OnYuukaOriginalSettings]\\n\\q[戻る,MainMenu]\\e");
  return make_response(b,outlen);
}

__declspec(dllexport) BOOL __cdecl load(HGLOBAL h,long len){
  char path[1200]; char* p=(char*)h; int i=0,n=(int)len;
  if(n<0)n=0; while(p&&i<n&&i<1023&&p[i]){g_dir[i]=p[i];++i;} g_dir[i]=0;
  load_feature();ensure_daily_state();build_core_path(path,(int)sizeof(path),g_dir);
  g_mod=LoadLibraryA(path);
  if(!g_mod){if(h)GlobalFree(h);return 0;}
  g_load=(load_fn)GetProcAddress(g_mod,"load");
  g_request=(request_fn)GetProcAddress(g_mod,"request");
  g_unload=(unload_fn)GetProcAddress(g_mod,"unload");
  if(!g_load||!g_request||!g_unload){if(h)GlobalFree(h);FreeLibrary(g_mod);g_mod=0;return 0;}
  return g_load(h,len);
}

static void force_reload(void){long rn=0;HGLOBAL r=core_event("OnYuukaReloadEvents",&rn);if(r)GlobalFree(r);}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  int inlen=(lenp?(int)*lenp:0);const char* event=0;unsigned long secs,count,slot,m,d,lastyear,state,statuscount;int affection,bm,bd;HGLOBAL r;long rn=0;SYSTEMTIME now;char input[64];
  if(!g_request){if(h)GlobalFree(h);if(lenp)*lenp=0;return 0;}
  force_reload();ensure_daily_state();

  if(h&&request_has_id((const char*)h,inlen,"OnYuukaSettings")){
    r=birthday_settings(&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(h&&request_has_id((const char*)h,inlen,"OnYuukaOriginalSettings")){
    r=core_event("OnYuukaSettings",&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(h&&request_has_id((const char*)h,inlen,"OnYuukaTodayCondition")){
    state=u32(g_feature+52);event=daily_state_event(state);
    if(event)r=core_event(event,&rn);
    else r=make_response("\\0\\s[1]今日はいつも通りです♪\\w5のんびりお話しできそうですよ。\\w5{name}さんは、今日はどんな調子ですか？\\e",&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(h&&request_has_id((const char*)h,inlen,"OnYuukaBirthdayInput")){
    input[0]=0;get_header_value((const char*)h,inlen,"Reference0:",input,(int)sizeof(input));
    if(parse_birthday(input,&bm,&bd)){
      p32(g_feature+36,(unsigned long)bm);p32(g_feature+40,(unsigned long)bd);p32(g_feature+44,0);save_feature_data();
      r=core_event("OnYuukaFeatureBirthdaySet",&rn);
    }else r=core_event("OnYuukaFeatureBirthdayInvalid",&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(h&&request_has_id((const char*)h,inlen,"OnYuukaBirthdayClear")){
    p32(g_feature+36,0);p32(g_feature+40,0);p32(g_feature+44,0);save_feature_data();
    r=core_event("OnYuukaFeatureBirthdayCleared",&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }

  if(h&&request_has_id((const char*)h,inlen,"OnBoot")){
    GetLocalTime(&now);m=u32(g_feature+36);d=u32(g_feature+40);lastyear=u32(g_feature+44);
    if(valid_birthday((int)m,(int)d)&&m==now.wMonth&&d==now.wDay&&lastyear!=(unsigned long)now.wYear){
      p32(g_feature+44,(unsigned long)now.wYear);save_feature_data();
      r=core_event("OnYuukaFeatureBirthdayToday",&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
    }
    if(g_has_last_seen){
      secs=away_seconds();
      if(secs<=60ul){
        r=make_response("\\0\\s[3]わっ！？\\w5もう戻ってきたんですか？\\w5えへへ……ちょっとびっくりしました。\\w5でも、またすぐ会えて嬉しいです♪\\e",&rn);
        if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
      }
      if(secs>=2592000ul)event="OnYuukaFeatureReturnLong";
      else if(secs>=604800ul)event="OnYuukaFeatureReturnWeek";
      else if(secs>=172800ul)event="OnYuukaFeatureReturnDays";
      else if(secs>=21600ul)event="OnYuukaFeatureReturnHours";
      else if(secs>=3600ul)event="OnYuukaFeatureReturnShort";
      if(event){r=core_event(event,&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}}
    }
  }

  if(h&&request_has_id((const char*)h,inlen,"OnYuukaTalk")){
    state=u32(g_feature+52);statuscount=u32(g_feature+56)+1ul;p32(g_feature+56,statuscount);save_feature_data();
    event=daily_state_event(state);
    if(event&&(statuscount%5ul)==0ul){
      r=core_event(event,&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
    }

    affection=query_affection();
    if(affection>=20){
      count=u32(g_feature+32)+1ul;p32(g_feature+32,count);save_feature_data();
      if((count%4ul)==0ul){
        slot=count/4ul-1ul;
        if(affection>=80){
          if((slot%3ul)==0ul)event="OnYuukaFeatureBond20";
          else if((slot%3ul)==1ul)event="OnYuukaFeatureBond50";
          else event="OnYuukaFeatureBond80";
        }else if(affection>=50){
          event=((slot%2ul)==0ul)?"OnYuukaFeatureBond20":"OnYuukaFeatureBond50";
        }else event="OnYuukaFeatureBond20";
        r=core_event(event,&rn);if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
      }
    }
  }

  return g_request(h,lenp);
}

__declspec(dllexport) BOOL __cdecl unload(void){
  BOOL r=TRUE;save_last_seen();if(g_unload)r=g_unload();if(g_mod)FreeLibrary(g_mod);g_mod=0;g_load=0;g_request=0;g_unload=0;return r;
}
