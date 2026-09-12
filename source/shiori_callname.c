#include "shiori_callname_part1.inc"
#include "shiori_callname_part2.inc"
#include "shiori_callname_part3.inc"
#include "shiori_callname_part4.inc"

__declspec(dllexport) BOOL __cdecl load(HGLOBAL h,long len){
  char path[1200];char* p=(char*)h;int i=0,n=(int)len;
  if(n<0)n=0;while(p&&i<n&&i<1023&&p[i]){g_dir[i]=p[i];++i;}g_dir[i]=0;
  g_call_loaded=0;g_call_enabled=0;g_call_base[0]=0;g_call_name[0]=0;g_pending_base[0]=0;
  build_path(path,(int)sizeof(path),g_dir,"shiori_base.dll");
  g_base_mod=LoadLibraryA(path);
  if(!g_base_mod){if(h)GlobalFree(h);return FALSE;}
  g_base_load=(load_fn)GetProcAddress(g_base_mod,"load");
  g_base_request=(request_fn)GetProcAddress(g_base_mod,"request");
  g_base_unload=(unload_fn)GetProcAddress(g_base_mod,"unload");
  if(!g_base_load||!g_base_request||!g_base_unload){if(h)GlobalFree(h);FreeLibrary(g_base_mod);g_base_mod=0;return FALSE;}
  return g_base_load(h,len);
}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  int inlen=(lenp?(int)*lenp:0);HGLOBAL r,handled;
  if(!g_base_request){if(h)GlobalFree(h);if(lenp)*lenp=0;return 0;}
  if(!h)return g_base_request(h,lenp);

  load_callname();
  handled=handle_callname_event(h,lenp,inlen);if(handled)return handled;

  /* Changing the registered name invalidates the stored replacement base. */
  if(request_has_id((const char*)h,inlen,"OnYuukaSetName"))save_callname_record("","");

  r=g_base_request(h,lenp);
  return apply_callname(r,lenp);
}

__declspec(dllexport) BOOL __cdecl unload(void){
  BOOL r=TRUE;
  if(g_base_unload)r=g_base_unload();
  if(g_base_mod)FreeLibrary(g_base_mod);
  g_base_mod=0;g_base_load=0;g_base_request=0;g_base_unload=0;
  return r;
}
