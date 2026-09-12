/*
 * Feature 6 layer: detailed time-of-day boot greetings.
 *
 * Keep the already-tested wrapper intact and wrap only its exported request()
 * function. More specific boot reactions (birthday / quick return / long return)
 * stay higher priority by delegating those cases to the existing implementation.
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

__declspec(dllexport) HGLOBAL __cdecl request(HGLOBAL h,long* lenp){
  int inlen=(lenp?(int)*lenp:0);
  unsigned long secs,m,d,lastyear;
  const char* event;
  HGLOBAL r;
  long rn=0;
  SYSTEMTIME now;

  if(!h||!request_has_id((const char*)h,inlen,"OnBoot"))
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
