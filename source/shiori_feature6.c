/*
 * Feature 6 + 12 layer:
 * - detailed time-of-day boot greetings
 * - one-time greeting after a network update
 *
 * The update greeting is keyed by ghost/master/release.txt and stored in the
 * private yuuka_features.dat state at offset 60. A fresh install records the
 * current release silently, while an existing install greets once after the
 * release marker changes. OnUpdateComplete is preferred, with OnBoot as a
 * fallback for updates that replace the currently-loaded SHIORI DLL.
 */
#define request yuuka_request_before_time_greetings
#include "shiori_wrapper.c"
#undef request

static const char* yuuka_time_greeting_event(WORD hour){
  if(hour<=3)return "OnYuukaFeatureBootDeepNight";      /* 00:00-03:59 */
  if(hour<=5)return "OnYuukaFeatureBootDawn";           /* 04:00-05:59 */
  if(hour<=8)return "OnYuukaFeatureBootEarlyMorning";   /* 06:00-08:59 */
  if(hour<=11)return "BootMorning";                      /* 09:00-11:59 */
  if(hour<=13)return "OnYuukaFeatureBootNoon";           /* 12:00-13:59 */
  if(hour<=16)return "OnYuukaFeatureBootAfternoon";      /* 14:00-16:59 */
  if(hour<=20)return "OnYuukaFeatureBootPrimeEvening";  /* 17:00-20:59 */
  return "OnYuukaFeatureBootLateNight";                 /* 21:00-23:59 */
}

static unsigned long yuuka_release_marker(void){
  char path[1200],buf[32];
  HANDLE f;
  DWORD got=0;
  unsigned long v=0;
  int i,have=0;

  build_path(path,(int)sizeof(path),g_dir,"release.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!ReadFile(f,buf,(DWORD)(sizeof(buf)-1),&got,0))got=0;
  CloseHandle(f);
  if(!got)return 0;
  buf[got]=0;

  for(i=0;i<(int)got;++i){
    if(buf[i]>='0'&&buf[i]<='9'){
      v=v*10ul+(unsigned long)(buf[i]-'0');
      have=1;
    }else if(have)break;
  }
  return have?v:0;
}

static int yuuka_pending_update(unsigned long* marker){
  unsigned long current=yuuka_release_marker();
  unsigned long seen=u32(g_feature+60);

  if(marker)*marker=current;
  if(!current)return 0;

  /* A brand-new install must not pretend that installation itself was an update. */
  if(!seen&&!g_has_last_seen){
    p32(g_feature+60,current);
    save_feature_data();
    return 0;
  }

  return seen!=current;
}

static HGLOBAL yuuka_update_greeting(HGLOBAL h,long* lenp,unsigned long marker){
  HGLOBAL r;
  long rn=0;

  force_reload();
  r=core_event("OnYuukaFeatureUpdated",&rn);
  if(!r)return 0;

  p32(g_feature+60,marker);
  save_feature_data();
  GlobalFree(h);
  if(lenp)*lenp=rn;
  return r;
}

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  int inlen=(lenp?(int)*lenp:0);
  unsigned long secs,m,d,lastyear,release_marker;
  const char* event;
  HGLOBAL r;
  long rn=0;
  SYSTEMTIME now;

  if(!h)
    return yuuka_request_before_time_greetings(h,lenp);

  /*
   * Feature 12 gets the highest priority at the end of a real network update.
   * If this release replaced the loaded DLL, the next OnBoot is the fallback.
   */
  if(yuuka_pending_update(&release_marker)&&
     (request_has_id((const char*)h,inlen,"OnUpdateComplete")||
      request_has_id((const char*)h,inlen,"OnBoot"))){
    r=yuuka_update_greeting(h,lenp,release_marker);
    if(r)return r;
  }

  if(!request_has_id((const char*)h,inlen,"OnBoot"))
    return yuuka_request_before_time_greetings(h,lenp);

  /* Preserve the first-boot introduction. */
  if(!g_has_last_seen)
    return yuuka_request_before_time_greetings(h,lenp);

  GetLocalTime(&now);

  /* Birthday greeting is more important than a normal time greeting. */
  m=u32(g_feature+36);
  d=u32(g_feature+40);
  lastyear=u32(g_feature+44);
  if(valid_birthday((int)m,(int)d)&&m==now.wMonth&&d==now.wDay&&lastyear!=(unsigned long)now.wYear)
    return yuuka_request_before_time_greetings(h,lenp);

  /* Keep return reactions higher priority too. 61 sec-59 min uses time greeting. */
  secs=away_seconds();
  if(secs<=60ul||secs>=3600ul)
    return yuuka_request_before_time_greetings(h,lenp);

  force_reload();
  event=yuuka_time_greeting_event(now.wHour);
  r=core_event(event,&rn);
  if(r){
    GlobalFree(h);
    if(lenp)*lenp=rn;
    return r;
  }

  return yuuka_request_before_time_greetings(h,lenp);
}
