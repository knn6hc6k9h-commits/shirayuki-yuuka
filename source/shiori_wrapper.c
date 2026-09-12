typedef void* HGLOBAL;
typedef void* HMODULE;
typedef int BOOL;
typedef unsigned long SIZE_T;
#define TRUE 1

__declspec(dllimport) HGLOBAL __stdcall GlobalAlloc(unsigned int, SIZE_T);
__declspec(dllimport) HGLOBAL __stdcall GlobalFree(HGLOBAL);
__declspec(dllimport) HMODULE __stdcall LoadLibraryA(const char*);
__declspec(dllimport) void* __stdcall GetProcAddress(HMODULE,const char*);
__declspec(dllimport) BOOL __stdcall FreeLibrary(HMODULE);

typedef BOOL (__cdecl *load_fn)(HGLOBAL,long);
typedef BOOL (__cdecl *unload_fn)(void);
typedef HGLOBAL (__cdecl *request_fn)(HGLOBAL,long*);

static HMODULE g_mod=0;
static load_fn g_load=0;
static unload_fn g_unload=0;
static request_fn g_request=0;

static void copy_bytes(char* d,const char* s,int n){int i;for(i=0;i<n;++i)d[i]=s[i];}
static void build_core_path(char* out,int cap,const char* dir){int n=0,i=0;out[0]=0;while(dir&&dir[i]&&n<cap-1)out[n++]=dir[i++];if(n>0&&out[n-1]!='\\'&&out[n-1]!='/'){if(n<cap-1)out[n++]='\\';}i=0;{const char* f="shiori_core.dll";while(f[i]&&n<cap-1)out[n++]=f[i++];}out[n]=0;}

__declspec(dllexport) BOOL __cdecl load(HGLOBAL h,long len){
  char dir[1024], path[1200]; char* p=(char*)h; int i=0,n=(int)len;
  if(n<0)n=0; while(p&&i<n&&i<1023&&p[i]){dir[i]=p[i];++i;} dir[i]=0;
  build_core_path(path,(int)sizeof(path),dir);
  g_mod=LoadLibraryA(path);
  if(!g_mod){if(h)GlobalFree(h);return 0;}
  g_load=(load_fn)GetProcAddress(g_mod,"load");
  g_request=(request_fn)GetProcAddress(g_mod,"request");
  g_unload=(unload_fn)GetProcAddress(g_mod,"unload");
  if(!g_load||!g_request||!g_unload){if(h)GlobalFree(h);FreeLibrary(g_mod);g_mod=0;return 0;}
  return g_load(h,len);
}

static void force_reload(void){
  static const char req[]="GET SHIORI/3.0\r\nCharset: UTF-8\r\nSender: SSP\r\nSecurityLevel: local\r\nID: OnYuukaReloadEvents\r\n\r\n";
  long n=(long)(sizeof(req)-1); HGLOBAL h=GlobalAlloc(0,(SIZE_T)n); HGLOBAL r; long rn=n;
  if(!h||!g_request)return; copy_bytes((char*)h,req,(int)n); r=g_request(h,&rn); if(r)GlobalFree(r);
}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  if(!g_request){if(h)GlobalFree(h);if(lenp)*lenp=0;return 0;}
  force_reload();
  return g_request(h,lenp);
}

__declspec(dllexport) BOOL __cdecl unload(void){
  BOOL r=TRUE; if(g_unload)r=g_unload(); if(g_mod)FreeLibrary(g_mod); g_mod=0;g_load=0;g_request=0;g_unload=0; return r;
}
