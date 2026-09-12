/*
 * Shirayuki Yuuka call-name layer.
 *
 * This outer SHIORI wrapper loads shiori_base.dll (the existing Yuuka
 * feature wrapper), then adds a user-configurable way for Yuuka to address
 * the user without changing the core SHIORI's registered user name.
 *
 * Default behaviour remains "registered name + さん".  When the user chooses
 * a different form, this layer remembers the registered base name and the
 * complete call name in private yuuka_callname.dat, then rewrites outgoing
 * SakuraScript text from "<base>さん" to the selected call name.
 */

typedef void* HGLOBAL;
typedef void* HMODULE;
typedef void* HANDLE;
typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned long SIZE_T;

#define TRUE 1
#define FALSE 0
#define GPTR 0x0040u
#define GENERIC_READ 0x80000000ul
#define GENERIC_WRITE 0x40000000ul
#define FILE_SHARE_READ 1ul
#define OPEN_EXISTING 3ul
#define CREATE_ALWAYS 2ul
#define FILE_ATTRIBUTE_NORMAL 0x80ul
#define INVALID_HANDLE_VALUE ((HANDLE)(-1))

__declspec(dllimport) HGLOBAL __stdcall GlobalAlloc(unsigned int, SIZE_T);
__declspec(dllimport) HGLOBAL __stdcall GlobalFree(HGLOBAL);
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HMODULE,const char*);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE);
__declspec(dllimport) HANDLE __stdcall CreateFileA(const char*,DWORD,DWORD,void*,DWORD,DWORD,HANDLE);
__declspec(dllimport) BOOL __stdcall ReadFile(HANDLE,void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall WriteFile(HANDLE,const void*,DWORD,DWORD*,void*);
__declspec(dllimport) BOOL __stdcall CloseHandle(HANDLE);

typedef BOOL (__cdecl *load_fn)(HGLOBAL,long);
typedef BOOL (__cdecl *unload_fn)(void);
typedef HGLOBAL (__cdecl *request_fn)(HGLOBAL,long*);

#define CALL_BASE_MAX 96
#define CALL_NAME_MAX 160

static HMODULE g_base_mod=0;
static load_fn g_base_load=0;
static unload_fn g_base_unload=0;
static request_fn g_base_request=0;
static char g_dir[1024];

static int g_call_loaded=0;
static int g_call_enabled=0;
static char g_call_base[CALL_BASE_MAX+1];
static char g_call_name[CALL_NAME_MAX+1];
static char g_pending_base[CALL_BASE_MAX+1];

static int slen(const char* s){int n=0;while(s&&s[n])++n;return n;}
static void copy_bytes(char* d,const char* s,int n){int i;for(i=0;i<n;++i)d[i]=s[i];}
static unsigned long u32(const unsigned char* p){return (unsigned long)p[0]|((unsigned long)p[1]<<8)|((unsigned long)p[2]<<16)|((unsigned long)p[3]<<24);}
static void p32(unsigned char* p,unsigned long v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}

static void build_path(char* out,int cap,const char* dir,const char* file){
  int n=0,i=0;out[0]=0;
  while(dir&&dir[i]&&n<cap-1)out[n++]=dir[i++];
  if(n>0&&out[n-1]!='\\'&&out[n-1]!='/'){if(n<cap-1)out[n++]='\\';}
  i=0;while(file&&file[i]&&n<cap-1)out[n++]=file[i++];out[n]=0;
}

static void append_text(char* out,int cap,int* pos,const char* s){
  int i=0;while(s&&s[i]&&*pos<cap-1)out[(*pos)++]=s[i++];out[*pos]=0;
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
    if(i==0||p[i-1]=='\n'){
      for(j=0;j<k&&i+j<n&&p[i+j]==key[j];++j){}
      if(j==k){
        i+=k;if(i<n&&p[i]==' ')++i;
        while(i<n&&p[i]!='\r'&&p[i]!='\n'&&w<cap-1)out[w++]=p[i++];
        out[w]=0;return 1;
      }
    }
  }
  return 0;
}

static HGLOBAL make_response(const char* value,long* outlen){
  static const char a[]="SHIORI/3.0 200 OK\r\nCharset: UTF-8\r\nSender: ShirayukiYuukaCallName\r\nValue: ";
  static const char e[]="\r\n\r\n";
  int na=(int)(sizeof(a)-1),nv=slen(value),ne=(int)(sizeof(e)-1),n=na+nv+ne;
  char* p=(char*)GlobalAlloc(GPTR,(SIZE_T)n+1);
  if(!p){if(outlen)*outlen=0;return 0;}
  copy_bytes(p,a,na);copy_bytes(p+na,value,nv);copy_bytes(p+na+nv,e,ne);p[n]=0;
  if(outlen)*outlen=n;return p;
}

static int clean_input(const char* src,char* dst,int cap){
  int i=0,p=0;
  unsigned char c;
  if(!src||!dst||cap<2)return 0;
  while(src[i]==' '||src[i]=='\t')++i;
  while(src[i]&&p<cap-1){
    c=(unsigned char)src[i++];
    if(c=='\r'||c=='\n')break;
    if(c<0x20)continue;
    /* A raw SakuraScript escape could alter the response, so do not store it. */
    if(c=='\\')continue;
    dst[p++]=(char)c;
  }
  while(p>0&&(dst[p-1]==' '||dst[p-1]=='\t'))--p;
  dst[p]=0;
  return p>0;
}
