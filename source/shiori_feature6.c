/*
 * Feature 6 + 12 + random-talk settings layer:
 * - detailed time-of-day boot greetings
 * - greeting when a network update completes
 * - random-talk interval can be changed from Yuuka's settings menu
 *
 * OnUpdateComplete always shows the update greeting.  The release marker in
 * ghost/master/release.txt is stored in private yuuka_features.dat at offset
 * 60 and is used only as an OnBoot fallback when a just-replaced SHIORI DLL
 * could not answer the completion event.  A fresh install records the current
 * release silently so installation itself is not mistaken for an update.
 *
 * A random-talk interval selected from the menu is stored at offset 32 in the
 * private feature file.  The public user_config.txt remains the core SHIORI's
 * source of truth while running; the stored value is only used to restore the
 * user's menu choice after an update replaces user_config.txt.
 */
#define request yuuka_request_before_time_greetings
#include "shiori_wrapper.c"
#undef request

#define YUUKA_RANDOM_TALK_DEFAULT 120ul
#define YUUKA_RANDOM_TALK_STORE_OFFSET 32

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

static int yuuka_random_talk_choice(unsigned long seconds){
  return seconds==30ul||seconds==60ul||seconds==120ul||seconds==180ul||seconds==300ul||seconds==600ul;
}

static unsigned long yuuka_read_random_talk_seconds(void){
  static const char key[]="random_talk_seconds=";
  char path[1200],buf[4096];
  HANDLE f;
  DWORD got=0;
  int i,j,keylen=(int)(sizeof(key)-1),have;
  unsigned long value;

  build_path(path,(int)sizeof(path),g_dir,"user_config.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return YUUKA_RANDOM_TALK_DEFAULT;
  if(!ReadFile(f,buf,(DWORD)(sizeof(buf)-1),&got,0))got=0;
  CloseHandle(f);
  if(!got)return YUUKA_RANDOM_TALK_DEFAULT;
  buf[got]=0;

  for(i=0;i+keylen<=(int)got;++i){
    if(i!=0&&buf[i-1]!='\n')continue;
    for(j=0;j<keylen&&buf[i+j]==key[j];++j){}
    if(j!=keylen)continue;
    i+=keylen;
    value=0;have=0;
    while(i<(int)got&&buf[i]>='0'&&buf[i]<='9'){
      value=value*10ul+(unsigned long)(buf[i]-'0');
      have=1;++i;
    }
    if(have&&value>0ul)return value;
    break;
  }
  return YUUKA_RANDOM_TALK_DEFAULT;
}

static int yuuka_write_random_talk_seconds(unsigned long seconds){
  static const char key[]="random_talk_seconds=";
  char path[1200],in[4096],out[4300];
  HANDLE f;
  DWORD got=0,wrote=0;
  int i,j,keylen=(int)(sizeof(key)-1),keypos=-1,skip,p=0;

  build_path(path,(int)sizeof(path),g_dir,"user_config.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!ReadFile(f,in,(DWORD)(sizeof(in)-1),&got,0))got=0;
  CloseHandle(f);
  if(!got)return 0;

  for(i=0;i+keylen<=(int)got;++i){
    if(i!=0&&in[i-1]!='\n')continue;
    for(j=0;j<keylen&&in[i+j]==key[j];++j){}
    if(j==keylen){keypos=i;break;}
  }

  if(keypos>=0){
    for(i=0;i<keypos+keylen&&p<(int)sizeof(out)-1;++i)out[p++]=in[i];
    skip=keypos+keylen;
    while(skip<(int)got&&in[skip]>='0'&&in[skip]<='9')++skip;
    append_uint(out,(int)sizeof(out),&p,seconds);
    for(i=skip;i<(int)got&&p<(int)sizeof(out)-1;++i)out[p++]=in[i];
  }else{
    for(i=0;i<(int)got&&p<(int)sizeof(out)-1;++i)out[p++]=in[i];
    if(p>0&&out[p-1]!='\n'&&p<(int)sizeof(out)-1)out[p++]='\n';
    append_text(out,(int)sizeof(out),&p,key);
    append_uint(out,(int)sizeof(out),&p,seconds);
    if(p<(int)sizeof(out)-1)out[p++]='\n';
  }

  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!WriteFile(f,out,(DWORD)p,&wrote,0))wrote=0;
  CloseHandle(f);
  return wrote==(DWORD)p;
}

static void yuuka_append_random_talk_interval(char* b,int cap,int* p,unsigned long seconds){
  if(seconds>=60ul&&seconds%60ul==0ul){
    append_uint(b,cap,p,seconds/60ul);
    append_text(b,cap,p,"分ごと");
  }else{
    append_uint(b,cap,p,seconds);
    append_text(b,cap,p,"秒ごと");
  }
}

static int yuuka_contains_ascii(const char* s,long n,const char* needle){
  int i,j,k=slen(needle);
  if(!s||n<=0||k<=0)return 0;
  for(i=0;i+k<=n;++i){
    for(j=0;j<k&&s[i+j]==needle[j];++j){}
    if(j==k)return 1;
  }
  return 0;
}

static HGLOBAL yuuka_main_menu(int mood,long* outlen){
  char b[1800];
  int p=0;
  b[0]=0;

  if(mood==2)append_text(b,sizeof(b),&p,"\\0\\s[4]……何ですか？\\n\\n");
  else if(mood==1)append_text(b,sizeof(b),&p,"\\0\\s[0]……どうしましたか？\\n\\n");
  else append_text(b,sizeof(b),&p,"\\0\\s[1]はいっ、どうしましたか？\\n\\n");

  append_text(b,sizeof(b),&p,"\\q[お話しする,OnYuukaTalk]\\n");
  append_text(b,sizeof(b),&p,"\\q[ゆうかに話しかける,OnYuukaSpeak]\\n");
  append_text(b,sizeof(b),&p,"\\q[今の気持ちを聞く,OnYuukaAffection]\\n");
  append_text(b,sizeof(b),&p,"\\q[設定,OnYuukaSettings]\\n");
  append_text(b,sizeof(b),&p,"\\q[感想を送る（Web拍手）,OnYuukaFeedback]\\n");
  append_text(b,sizeof(b),&p,"\\q[閉じる,script:\\e]\\*\\e");
  return make_response(b,outlen);
}

static HGLOBAL yuuka_settings_menu(long* outlen){
  char b[1600];
  int p=0;
  b[0]=0;

  append_text(b,sizeof(b),&p,"\\0\\s[1]設定ですね♪\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[呼び方を決める,OnYuukaCallNameSettings]\\n");
  append_text(b,sizeof(b),&p,"\\q[ランダムトークの頻度を変更,OnYuukaRandomTalkSettings]\\n");
  append_text(b,sizeof(b),&p,"\\q[誕生日を変更,OnYuukaBirthdaySettings]\\n");
  append_text(b,sizeof(b),&p,"\\q[その他設定,OnYuukaOriginalSettings]\\n");
  append_text(b,sizeof(b),&p,"\\q[戻る,MainMenu]\\e");
  return make_response(b,outlen);
}

static HGLOBAL yuuka_birthday_settings_menu(long* outlen){
  char b[1400];
  int p=0;
  unsigned long m=u32(g_feature+36),d=u32(g_feature+40);
  b[0]=0;

  append_text(b,sizeof(b),&p,"\\0\\s[1]誕生日の設定ですね♪\\n\\n現在：");
  if(valid_birthday((int)m,(int)d)){
    append_uint(b,sizeof(b),&p,m);
    append_text(b,sizeof(b),&p,"月");
    append_uint(b,sizeof(b),&p,d);
    append_text(b,sizeof(b),&p,"日");
  }else{
    append_text(b,sizeof(b),&p,"未登録");
  }
  append_text(b,sizeof(b),&p,"\\n\\n\\q[誕生日を変更,OnYuukaFeatureBirthdayPrompt]\\n");
  append_text(b,sizeof(b),&p,"\\q[設定に戻る,OnYuukaSettings]\\e");
  return make_response(b,outlen);
}

static HGLOBAL yuuka_other_settings_menu(long* outlen){
  char b[1200];
  int p=0,paused=0;
  long rn=0;
  HGLOBAL current=core_event("OnYuukaSettings",&rn);

  if(current){
    paused=yuuka_contains_ascii((const char*)current,rn,"OFF");
    GlobalFree(current);
  }

  b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]その他設定です。\\n\\n");
  if(paused)append_text(b,sizeof(b),&p,"\\q[カウント増加を再開,OnYuukaToggleHarassmentCount]\\n\\n");
  else append_text(b,sizeof(b),&p,"\\q[カウント増加を停止,OnYuukaToggleHarassmentCount]\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[設定に戻る,OnYuukaSettings]\\e");
  return make_response(b,outlen);
}

static HGLOBAL yuuka_random_talk_settings_menu(long* outlen){
  char b[1500];
  int p=0;
  unsigned long seconds=yuuka_read_random_talk_seconds();
  b[0]=0;

  append_text(b,sizeof(b),&p,"\\0\\s[1]ランダムトークの頻度ですね♪\\n今は ");
  yuuka_append_random_talk_interval(b,sizeof(b),&p,seconds);
  append_text(b,sizeof(b),&p," くらいです。\\n\\nどのくらいの間隔にしますか？\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[30秒（かなり多め）,OnYuukaRandomTalk30]\\n");
  append_text(b,sizeof(b),&p,"\\q[1分,OnYuukaRandomTalk60]\\n");
  append_text(b,sizeof(b),&p,"\\q[2分（標準）,OnYuukaRandomTalk120]\\n");
  append_text(b,sizeof(b),&p,"\\q[3分,OnYuukaRandomTalk180]\\n");
  append_text(b,sizeof(b),&p,"\\q[5分,OnYuukaRandomTalk300]\\n");
  append_text(b,sizeof(b),&p,"\\q[10分（少なめ）,OnYuukaRandomTalk600]\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[設定に戻る,OnYuukaSettings]\\e");
  return make_response(b,outlen);
}

static HGLOBAL yuuka_set_random_talk_interval(unsigned long seconds,long* outlen){
  char b[1000];
  int p=0;

  if(!yuuka_random_talk_choice(seconds)||!yuuka_write_random_talk_seconds(seconds))
    return make_response("\\0\\s[2]ご、ごめんなさい……設定の保存に失敗しました。\\w5もう一度試してみてください。\\n\\q[設定に戻る,OnYuukaSettings]\\e",outlen);

  p32(g_feature+YUUKA_RANDOM_TALK_STORE_OFFSET,seconds);
  save_feature_data();
  force_reload();

  b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]はいっ♪\\w5ランダムトークは、だいたい ");
  yuuka_append_random_talk_interval(b,sizeof(b),&p,seconds);
  append_text(b,sizeof(b),&p," にしますね。\\w5また変えたくなったら、設定からいつでも変えられますよ♪\\n\\q[設定に戻る,OnYuukaSettings]\\e");
  return make_response(b,outlen);
}

static void yuuka_restore_random_talk_choice(void){
  unsigned long stored=u32(g_feature+YUUKA_RANDOM_TALK_STORE_OFFSET);
  unsigned long current;
  if(!yuuka_random_talk_choice(stored))return;
  current=yuuka_read_random_talk_seconds();
  if(current!=stored&&yuuka_write_random_talk_seconds(stored))force_reload();
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

  if(marker){
    p32(g_feature+60,marker);
    save_feature_data();
  }
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

  /* Keep the visible menus small and predictable. */
  if(request_has_id((const char*)h,inlen,"MainMenu")){
    r=yuuka_main_menu(0,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"MainMenuGrumpy")){
    r=yuuka_main_menu(1,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"MainMenuAngry")){
    r=yuuka_main_menu(2,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaSettings")){
    force_reload();
    r=yuuka_settings_menu(&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaBirthdaySettings")){
    r=yuuka_birthday_settings_menu(&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaOriginalSettings")){
    r=yuuka_other_settings_menu(&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaToggleHarassmentCount")){
    long base_len=inlen;
    HGLOBAL base_response=yuuka_request_before_time_greetings(h,&base_len);
    if(base_response)GlobalFree(base_response);
    r=yuuka_other_settings_menu(&rn);
    if(r){if(lenp)*lenp=rn;return r;}
    if(lenp)*lenp=0;
    return 0;
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalkSettings")){
    r=yuuka_random_talk_settings_menu(&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk30")){
    r=yuuka_set_random_talk_interval(30ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk60")){
    r=yuuka_set_random_talk_interval(60ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk120")){
    r=yuuka_set_random_talk_interval(120ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk180")){
    r=yuuka_set_random_talk_interval(180ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk300")){
    r=yuuka_set_random_talk_interval(300ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }
  if(request_has_id((const char*)h,inlen,"OnYuukaRandomTalk600")){
    r=yuuka_set_random_talk_interval(600ul,&rn);
    if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}
  }

  /* Restore a menu-selected interval if a network update replaced user_config.txt. */
  if(request_has_id((const char*)h,inlen,"OnBoot"))yuuka_restore_random_talk_choice();

  /*
   * A completed network update should always get a completion line.
   * release.txt is only used to recover that greeting on the next OnBoot when
   * the update replaced the currently-loaded SHIORI before it could respond.
   */
  if(request_has_id((const char*)h,inlen,"OnUpdateComplete")){
    release_marker=yuuka_release_marker();
    r=yuuka_update_greeting(h,lenp,release_marker);
    if(r)return r;
  }

  if(request_has_id((const char*)h,inlen,"OnBoot")&&
     yuuka_pending_update(&release_marker)){
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