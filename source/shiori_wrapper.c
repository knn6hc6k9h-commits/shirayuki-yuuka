/*
  Shirayuki Yuuka SHIORI feature wrapper v2.0
  Delegates ordinary behavior to shiori_core.dll while adding persistent
  companion features: return greetings, birthday, affinity unlocks,
  daily condition, fine-grained time greetings, and post-update greeting.
*/

typedef void* HGLOBAL;
typedef void* HMODULE;
typedef void* HANDLE;
typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned long SIZE_T;
typedef unsigned short WORD;

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
__declspec(dllimport) DWORD __stdcall GetFileSize(HANDLE,DWORD*);
__declspec(dllimport) void __stdcall GetLocalTime(SYSTEMTIME*);

#define TRUE 1
#define FALSE 0
#define GPTR 0x0040u
#define GENERIC_READ  0x80000000ul
#define GENERIC_WRITE 0x40000000ul
#define FILE_SHARE_READ 1ul
#define OPEN_EXISTING 3ul
#define CREATE_ALWAYS 2ul
#define FILE_ATTRIBUTE_NORMAL 0x80ul
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))

typedef BOOL (__cdecl *load_fn)(HGLOBAL,long);
typedef BOOL (__cdecl *unload_fn)(void);
typedef HGLOBAL (__cdecl *request_fn)(HGLOBAL,long*);

static HMODULE g_mod=0;
static load_fn g_load=0;
static unload_fn g_unload=0;
static request_fn g_request=0;
static char g_dir[768];
static char g_feature_path[896];
static unsigned long g_last_y=0,g_last_m=0,g_last_d=0,g_last_h=0,g_last_min=0;
static unsigned long g_birth_m=0,g_birth_d=0,g_birth_greet_y=0,g_seen_release=0;
static unsigned long g_current_release=2026091202ul;
static unsigned long g_counter=1;
static int g_lifestyle=0;

static int slen(const char* s){int n=0;if(!s)return 0;while(s[n])++n;return n;}
static int seq(const char* a,const char* b){int i=0;if(!a||!b)return 0;while(a[i]&&b[i]){if(a[i]!=b[i])return 0;++i;}return a[i]==0&&b[i]==0;}
static void cpy(char* d,const char* s,int n){int i;for(i=0;i<n;++i)d[i]=s[i];}
static void add(char* d,int cap,const char* s){int p=slen(d),i=0;while(s&&s[i]&&p<cap-1)d[p++]=s[i++];d[p]=0;}
static void addnum(char* d,int cap,unsigned long v){char t[16];int n=0,i;if(!v){add(d,cap,"0");return;}while(v&&n<15){t[n++]=(char)('0'+v%10);v/=10;}for(i=n-1;i>=0;--i){char c[2];c[0]=t[i];c[1]=0;add(d,cap,c);}}
static unsigned long u32(const unsigned char* p){return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);}
static void p32(unsigned char* p,unsigned long v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}

static int header(const char* req,int len,const char* key,char* out,int cap){
  int i=0,k=slen(key);if(cap)out[0]=0;
  while(i<len){int st=i,en,j,p;while(i<len&&req[i]!='\r'&&req[i]!='\n')++i;en=i;while(i<len&&(req[i]=='\r'||req[i]=='\n'))++i;if(en-st<k+1)continue;for(j=0;j<k;++j)if(req[st+j]!=key[j])break;if(j!=k||req[st+k]!=':')continue;p=st+k+1;while(p<en&&(req[p]==' '||req[p]=='\t'))++p;j=0;while(p<en&&j<cap-1)out[j++]=req[p++];if(cap)out[j]=0;return 1;}
  return 0;
}
static void makepath(char* out,int cap,const char* name){int i=0,p=0;while(g_dir[i]&&p<cap-1)out[p++]=g_dir[i++];if(p&&out[p-1]!='\\'&&out[p-1]!='/')out[p++]='\\';i=0;while(name[i]&&p<cap-1)out[p++]=name[i++];out[p]=0;}
static int readfile(const char* name,unsigned char* b,int cap){char p[896];HANDLE f;DWORD n,g=0;makepath(p,896,name);f=CreateFileA(p,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return 0;n=GetFileSize(f,0);if(n>(DWORD)cap)n=(DWORD)cap;if(!ReadFile(f,b,n,&g,0))g=0;CloseHandle(f);return (int)g;}

static void build_core_path(char* out,int cap){makepath(out,cap,"shiori_core.dll");}
static void build_feature_path(void){makepath(g_feature_path,(int)sizeof(g_feature_path),"yuuka_features.dat");}
static void update_seen_now(void){SYSTEMTIME s;GetLocalTime(&s);g_last_y=s.wYear;g_last_m=s.wMonth;g_last_d=s.wDay;g_last_h=s.wHour;g_last_min=s.wMinute;}
static int leap(unsigned long y){return (y%4==0&&y%100!=0)||y%400==0;}
static unsigned long daystamp(unsigned long y,unsigned long m,unsigned long d){static const unsigned char md[12]={31,28,31,30,31,30,31,31,30,31,30,31};unsigned long x=0,yy,mm;if(y<2000||m<1||m>12||d<1||d>31)return 0;for(yy=2000;yy<y;++yy)x+=leap(yy)?366:365;for(mm=1;mm<m;++mm){x+=md[mm-1];if(mm==2&&leap(y))++x;}return x+d-1;}
static unsigned long away_minutes(void){SYSTEMTIME s;unsigned long a,b;GetLocalTime(&s);a=daystamp(g_last_y,g_last_m,g_last_d);b=daystamp(s.wYear,s.wMonth,s.wDay);if(!a||!b||b<a)return 0;return (b-a)*1440ul+(unsigned long)s.wHour*60ul+s.wMinute-(g_last_h*60ul+g_last_min);}
static void save_features(void){unsigned char b[48];DWORD w=0;HANDLE f;if(!g_feature_path[0])return;b[0]='Y';b[1]='U';b[2]='K';b[3]='F';p32(b+4,1);p32(b+8,g_last_y);p32(b+12,g_last_m);p32(b+16,g_last_d);p32(b+20,g_last_h);p32(b+24,g_last_min);p32(b+28,g_birth_m);p32(b+32,g_birth_d);p32(b+36,g_birth_greet_y);p32(b+40,g_seen_release);p32(b+44,(unsigned long)g_lifestyle);f=CreateFileA(g_feature_path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return;WriteFile(f,b,48,&w,0);CloseHandle(f);}
static void load_features(void){unsigned char b[48];HANDLE f;DWORD n,g=0;g_last_y=g_last_m=g_last_d=g_last_h=g_last_min=0;g_birth_m=g_birth_d=g_birth_greet_y=g_seen_release=0;g_lifestyle=0;if(!g_feature_path[0])return;f=CreateFileA(g_feature_path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE)return;n=GetFileSize(f,0);if(n>48)n=48;if(!ReadFile(f,b,n,&g,0))g=0;CloseHandle(f);if(g<44||b[0]!='Y'||b[1]!='U'||b[2]!='K'||b[3]!='F')return;g_last_y=u32(b+8);g_last_m=u32(b+12);g_last_d=u32(b+16);g_last_h=u32(b+20);g_last_min=u32(b+24);g_birth_m=u32(b+28);g_birth_d=u32(b+32);g_birth_greet_y=u32(b+36);g_seen_release=u32(b+40);if(g>=48)g_lifestyle=(int)u32(b+44);}
static void load_release(void){unsigned char b[64];int n=readfile("release.txt",b,63),i=0;unsigned long v=0;if(n<=0)return;b[n]=0;while(i<n&&b[i]>='0'&&b[i]<='9'){v=v*10ul+(unsigned long)(b[i]-'0');++i;}if(v)g_current_release=v;}

static void read_core_state(unsigned long* affection,unsigned long* harassment,int* enabled){unsigned char b[224];int n=readfile("yuuka_state.dat",b,224);unsigned long nl,ver;*affection=0;*harassment=0;*enabled=1;if(n<20||b[0]!='Y'||b[1]!='U'||b[2]!='K'||b[3]!='A')return;ver=u32(b+4);*affection=u32(b+8);nl=u32(b+16);if(nl>127)nl=127;if(ver>=3&&20+nl+4<=(unsigned long)n)*harassment=u32(b+20+nl);if(ver>=4&&24+nl+4<=(unsigned long)n)*enabled=u32(b+24+nl)?1:0;}
static unsigned long rnd(void){SYSTEMTIME s;GetLocalTime(&s);g_counter=g_counter*1664525ul+1013904223ul+s.wMilliseconds+s.wSecond;return g_counter;}
static void choose_lifestyle(unsigned long affection){SYSTEMTIME s;unsigned long r;GetLocalTime(&s);r=s.wDay+s.wMonth*37ul+affection+g_counter;if(s.wHour<6)g_lifestyle=1;else if(s.wHour>=11&&s.wHour<=13&&(r%3)==0)g_lifestyle=3;else g_lifestyle=(int)(r%5);save_features();}
static const char* life_name(void){if(g_lifestyle==1)return "眠そう";if(g_lifestyle==2)return "元気いっぱい";if(g_lifestyle==3)return "お腹ぺこぺこ";if(g_lifestyle==4)return "読書気分";return "いつも通り";}
static int valid_birth(unsigned long m,unsigned long d){static const unsigned char md[12]={31,29,31,30,31,30,31,31,30,31,30,31};return m>=1&&m<=12&&d>=1&&d<=md[m-1];}
static int parse_birth(const char* s,unsigned long* m,unsigned long* d){unsigned long a=0,b=0;int i=0,na=0,nb=0;while(s[i]&&!(s[i]>='0'&&s[i]<='9'))++i;while(s[i]>='0'&&s[i]<='9'){a=a*10+s[i]-'0';++i;++na;}while(s[i]&&!(s[i]>='0'&&s[i]<='9'))++i;while(s[i]>='0'&&s[i]<='9'){b=b*10+s[i]-'0';++i;++nb;}if(!na||!nb||!valid_birth(a,b))return 0;*m=a;*d=b;return 1;}

static HGLOBAL make_response(const char* value,long* outlen){const char*a="SHIORI/3.0 200 OK\r\nCharset: UTF-8\r\nSender: ShirayukiYuuka\r\nValue: ";const char*e="\r\n\r\n";int na=slen(a),nv=slen(value),ne=slen(e),n=na+nv+ne;char*p=(char*)GlobalAlloc(GPTR,n+1);if(!p){*outlen=0;return 0;}cpy(p,a,na);cpy(p+na,value,nv);cpy(p+na+nv,e,ne);p[n]=0;*outlen=n;return p;}
static int response200(HGLOBAL h,long n){char*p=(char*)h;const char*x="SHIORI/3.0 200";int i;if(!p||n<14)return 0;for(i=0;x[i];++i)if(p[i]!=x[i])return 0;return 1;}
static HGLOBAL core_event(const char* id,long* outlen){char q[512];int p=0,i=0,n;HGLOBAL h;const char*a="GET SHIORI/3.0\r\nCharset: UTF-8\r\nSender: SSP\r\nSecurityLevel: local\r\nID: ",*e="\r\n\r\n";while(a[i])q[p++]=a[i++];i=0;while(id[i]&&p<490)q[p++]=id[i++];i=0;while(e[i])q[p++]=e[i++];n=p;h=GlobalAlloc(0,n);if(!h){*outlen=0;return 0;}cpy((char*)h,q,n);return g_request(h,outlen);}
static void force_reload(void){long n;HGLOBAL r=core_event("OnYuukaReloadEvents",&n);if(r)GlobalFree(r);}
static HGLOBAL replace_with_event(HGLOBAL old,const char* id,long* lenp){HGLOBAL r;if(old)GlobalFree(old);r=core_event(id,lenp);return r;}

static HGLOBAL settings(long* lenp){char s[1200];unsigned long a,h;int en;s[0]=0;read_core_state(&a,&h,&en);add(s,1200,"\\0\\s[0]設定です。\\n\\nセクハラカウントの増加：");add(s,1200,en?"ON":"OFF");add(s,1200,"\\n現在のカウント：");addnum(s,1200,h);add(s,1200,"\\n誕生日：");if(g_birth_m){addnum(s,1200,g_birth_m);add(s,1200,"月");addnum(s,1200,g_birth_d);add(s,1200,"日");}else add(s,1200,"未登録");add(s,1200,"\\n今日のゆうか：");add(s,1200,life_name());add(s,1200,"\\n\\n\\q[");add(s,1200,en?"カウント増加を停止する":"カウント増加を再開する");add(s,1200,",OnYuukaToggleHarassmentCount]\\n\\q[誕生日を登録・変更,OnYuukaSetBirthday]\\n");if(g_birth_m)add(s,1200,"\\q[誕生日登録を消す,OnYuukaClearBirthday]\\n");add(s,1200,"\\q[戻る,OnYuukaMenu]\\e");return make_response(s,lenp);}

__declspec(dllexport) BOOL __cdecl load(HGLOBAL h,long len){char path[1000];char*p=(char*)h;int i=0;if(len<0)len=0;while(p&&i<len&&i<767&&p[i]){g_dir[i]=p[i];++i;}g_dir[i]=0;build_feature_path();load_features();load_release();build_core_path(path,1000);g_mod=LoadLibraryA(path);if(!g_mod){if(h)GlobalFree(h);return FALSE;}g_load=(load_fn)GetProcAddress(g_mod,"load");g_request=(request_fn)GetProcAddress(g_mod,"request");g_unload=(unload_fn)GetProcAddress(g_mod,"unload");if(!g_load||!g_request||!g_unload){if(h)GlobalFree(h);FreeLibrary(g_mod);g_mod=0;return FALSE;}return g_load(h,len);}
__declspec(dllexport) BOOL __cdecl unload(void){BOOL r=TRUE;update_seen_now();save_features();if(g_unload)r=g_unload();if(g_mod)FreeLibrary(g_mod);g_mod=0;g_load=0;g_request=0;g_unload=0;return r;}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  char id[96],r0[256];char*req=(char*)h;int len=lenp?(int)*lenp:0,has=0;HGLOBAL r;unsigned long affection,harassment,away;int enabled;SYSTEMTIME st;const char*ev=0;
  id[0]=r0[0]=0;if(req&&len>0){has=header(req,len,"ID",id,96);header(req,len,"Reference0",r0,256);}if(!lenp)return 0;if(!g_request){if(h)GlobalFree(h);*lenp=0;return 0;}if(!has)return g_request(h,lenp);
  force_reload();load_release();
  if(seq(id,"OnYuukaSetBirthday")){if(h)GlobalFree(h);return core_event("OnYuukaFeatureBirthdayPrompt",lenp);}
  if(seq(id,"OnYuukaBirthdayInput")){if(h)GlobalFree(h);if(!parse_birth(r0,&g_birth_m,&g_birth_d))return core_event("OnYuukaFeatureBirthdayInvalid",lenp);g_birth_greet_y=0;save_features();return core_event("OnYuukaFeatureBirthdaySet",lenp);}
  if(seq(id,"OnYuukaClearBirthday")){if(h)GlobalFree(h);g_birth_m=g_birth_d=g_birth_greet_y=0;save_features();return core_event("OnYuukaFeatureBirthdayCleared",lenp);}
  if(seq(id,"OnYuukaSettings")){if(h)GlobalFree(h);return settings(lenp);}

  r=g_request(h,lenp);
  if(seq(id,"OnYuukaToggleHarassmentCount")){if(r)GlobalFree(r);return settings(lenp);}
  read_core_state(&affection,&harassment,&enabled);

  if(seq(id,"OnFirstBoot")){g_seen_release=g_current_release;choose_lifestyle(affection);update_seen_now();save_features();return r;}
  if(seq(id,"OnClose")){update_seen_now();save_features();return r;}
  if(seq(id,"OnUpdateComplete")){if(g_current_release&&g_seen_release!=g_current_release){g_seen_release=g_current_release;save_features();return replace_with_event(r,"OnYuukaFeatureUpdated",lenp);}return r;}
  if(seq(id,"OnBoot")){
    away=away_minutes();choose_lifestyle(affection);GetLocalTime(&st);
    if(g_current_release&&g_seen_release!=g_current_release){g_seen_release=g_current_release;ev="OnYuukaFeatureUpdated";}
    else if(g_birth_m==st.wMonth&&g_birth_d==st.wDay&&g_birth_greet_y!=st.wYear){g_birth_greet_y=st.wYear;ev="OnYuukaFeatureBirthdayToday";}
    else if(harassment<=20){
      if(away>=43200)ev="OnYuukaFeatureReturnLong";else if(away>=10080)ev="OnYuukaFeatureReturnWeek";else if(away>=2880)ev="OnYuukaFeatureReturnDays";else if(away>=360)ev="OnYuukaFeatureReturnHours";else if(away>=60)ev="OnYuukaFeatureReturnShort";
      else if(st.wHour<4)ev="OnYuukaFeatureBootDeepNight";else if(st.wHour<6)ev="OnYuukaFeatureBootDawn";else if(st.wHour<9)ev="OnYuukaFeatureBootEarlyMorning";else if(st.wHour>=11&&st.wHour<13)ev="OnYuukaFeatureBootNoon";else if(st.wHour>=13&&st.wHour<17)ev="OnYuukaFeatureBootAfternoon";else if(st.wHour>=19&&st.wHour<22)ev="OnYuukaFeatureBootPrimeEvening";else if(st.wHour>=22)ev="OnYuukaFeatureBootLateNight";
    }
    update_seen_now();save_features();if(ev)return replace_with_event(r,ev,lenp);return r;
  }
  if((seq(id,"OnYuukaTalk")||(seq(id,"OnSecondChange")&&response200(r,*lenp)))&&harassment<=20){
    unsigned long q=rnd()%12ul;
    if(q<3){if(affection>=80)ev="OnYuukaFeatureBond80";else if(affection>=50)ev="OnYuukaFeatureBond50";else if(affection>=20)ev="OnYuukaFeatureBond20";}
    if(!ev&&q>=3&&q<6){if(g_lifestyle==1)ev="OnYuukaFeatureSleepy";else if(g_lifestyle==2)ev="OnYuukaFeatureEnergetic";else if(g_lifestyle==3)ev="OnYuukaFeatureHungry";else if(g_lifestyle==4)ev="OnYuukaFeatureReading";}
    if(ev)return replace_with_event(r,ev,lenp);
  }
  return r;
}
