/*
 * Editable menu labels.
 *
 * menu_config.txt lives beside shiori.dll.  It is intentionally treated as a
 * local user setting so network updates do not overwrite customized labels.
 * If an older installation does not have the file yet, the defaults are
 * created automatically the first time a configurable menu is opened.
 */
#define YUUKA_MENU_LABEL_MAX 160

static const char yuuka_menu_config_defaults[]=
"# 白雪ゆうか メニュー項目名設定\r\n"
"# 「=」の右側だけを書き換えると、メニューに表示される名前を変更できます。\r\n"
"# 例: main_talk=ゆうかとおしゃべり\r\n"
"#\r\n"
"# 注意:\r\n"
"# ・キー名（= の左側）は変更しないでください。\r\n"
"# ・半角の \\  ,  [  ] は項目名に使用しないでください。\r\n"
"# ・保存後、次にメニューを開いたときから反映されます。\r\n"
"# ・このファイルはネットワーク更新では上書きしない設定ファイルです。\r\n"
"\r\n"
"# メインメニュー\r\n"
"main_talk=お話しする\r\n"
"main_speak=ゆうかに話しかける\r\n"
"main_affection=今の気持ちを聞く\r\n"
"main_settings=設定\r\n"
"main_feedback=感想を送る（Web拍手）\r\n"
"main_close=閉じる\r\n"
"\r\n"
"# 設定メニュー\r\n"
"settings_callname=呼び方を決める\r\n"
"settings_randomtalk=ランダムトークの頻度を変更\r\n"
"settings_birthday=誕生日を変更\r\n"
"settings_other=その他設定\r\n"
"settings_back=戻る\r\n"
"\r\n"
"# その他設定メニュー\r\n"
"other_count_stop=秘密のカウントの停止\r\n"
"other_count_resume=秘密のカウントの再開\r\n";

static void yuuka_copy_menu_text(char* out,int cap,const char* text){
  int i=0;
  if(!out||cap<1)return;
  while(text&&text[i]&&i<cap-1){out[i]=text[i];++i;}
  out[i]=0;
}

static void yuuka_ensure_menu_config(void){
  char path[1200];
  HANDLE f;
  DWORD wrote=0;
  build_path(path,(int)sizeof(path),g_dir,"menu_config.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f!=INVALID_HANDLE_VALUE){CloseHandle(f);return;}
  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;
  WriteFile(f,yuuka_menu_config_defaults,(DWORD)slen(yuuka_menu_config_defaults),&wrote,0);
  CloseHandle(f);
}

static int yuuka_menu_value_is_safe(const char* s,int n){
  int i;unsigned char c;
  if(!s||n<=0)return 0;
  for(i=0;i<n;++i){
    c=(unsigned char)s[i];
    if(c<0x20||c=='\\'||c==','||c=='['||c==']')return 0;
  }
  return 1;
}

static void yuuka_read_menu_label(const char* key,const char* fallback,char* out,int cap){
  char path[1200],buf[4096];
  HANDLE f;
  DWORD got=0;
  int i=0,start,end,pos,v,vend,j,k=slen(key),n;
  yuuka_copy_menu_text(out,cap,fallback);
  yuuka_ensure_menu_config();

  build_path(path,(int)sizeof(path),g_dir,"menu_config.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;
  if(!ReadFile(f,buf,(DWORD)(sizeof(buf)-1),&got,0))got=0;
  CloseHandle(f);
  if(!got)return;
  buf[got]=0;

  while(i<(int)got){
    start=i;
    while(i<(int)got&&buf[i]!='\r'&&buf[i]!='\n')++i;
    end=i;
    while(i<(int)got&&(buf[i]=='\r'||buf[i]=='\n'))++i;

    pos=start;
    while(pos<end&&(buf[pos]==' '||buf[pos]=='\t'))++pos;
    if(pos>=end||buf[pos]=='#')continue;
    for(j=0;j<k&&pos+j<end&&buf[pos+j]==key[j];++j){}
    if(j!=k||pos+k>=end||buf[pos+k]!='=')continue;

    v=pos+k+1;
    while(v<end&&(buf[v]==' '||buf[v]=='\t'))++v;
    vend=end;
    while(vend>v&&(buf[vend-1]==' '||buf[vend-1]=='\t'))--vend;
    n=vend-v;
    if(!yuuka_menu_value_is_safe(buf+v,n))return;
    if(n>=cap)n=cap-1;
    for(j=0;j<n;++j)out[j]=buf[v+j];
    out[n]=0;
    return;
  }
}

static HGLOBAL yuuka_replace_menu_item(HGLOBAL r,long* lenp,const char* key,const char* fallback,const char* event){
  char label[YUUKA_MENU_LABEL_MAX+1],needle[420],replacement[420];
  int p=0;
  if(!r)return r;
  yuuka_read_menu_label(key,fallback,label,(int)sizeof(label));
  if(slen(label)==slen(fallback)){
    int i=0;while(label[i]&&label[i]==fallback[i])++i;
    if(!label[i]&&!fallback[i])return r;
  }

  needle[0]=0;
  append_text(needle,(int)sizeof(needle),&p,"\\q[");
  append_text(needle,(int)sizeof(needle),&p,fallback);
  append_text(needle,(int)sizeof(needle),&p,",");
  append_text(needle,(int)sizeof(needle),&p,event);
  append_text(needle,(int)sizeof(needle),&p,"]");

  p=0;replacement[0]=0;
  append_text(replacement,(int)sizeof(replacement),&p,"\\q[");
  append_text(replacement,(int)sizeof(replacement),&p,label);
  append_text(replacement,(int)sizeof(replacement),&p,",");
  append_text(replacement,(int)sizeof(replacement),&p,event);
  append_text(replacement,(int)sizeof(replacement),&p,"]");
  return replace_all(r,lenp,needle,replacement);
}

static HGLOBAL yuuka_apply_menu_labels(HGLOBAL r,long* lenp,int main_menu,int settings_menu,int other_settings_menu){
  if(main_menu){
    r=yuuka_replace_menu_item(r,lenp,"main_talk","お話しする","OnYuukaTalk");
    r=yuuka_replace_menu_item(r,lenp,"main_speak","ゆうかに話しかける","OnYuukaSpeak");
    r=yuuka_replace_menu_item(r,lenp,"main_affection","今の気持ちを聞く","OnYuukaAffection");
    r=yuuka_replace_menu_item(r,lenp,"main_settings","設定","OnYuukaSettings");
    r=yuuka_replace_menu_item(r,lenp,"main_feedback","感想を送る（Web拍手）","OnYuukaFeedback");
    r=yuuka_replace_menu_item(r,lenp,"main_close","閉じる","script:\\e");
  }
  if(settings_menu){
    r=yuuka_replace_menu_item(r,lenp,"settings_callname","呼び方を決める","OnYuukaCallNameSettings");
    r=yuuka_replace_menu_item(r,lenp,"settings_randomtalk","ランダムトークの頻度を変更","OnYuukaRandomTalkSettings");
    r=yuuka_replace_menu_item(r,lenp,"settings_birthday","誕生日を変更","OnYuukaBirthdaySettings");
    r=yuuka_replace_menu_item(r,lenp,"settings_other","その他設定","OnYuukaOriginalSettings");
    r=yuuka_replace_menu_item(r,lenp,"settings_back","戻る","MainMenu");
  }
  if(other_settings_menu){
    r=yuuka_replace_menu_item(r,lenp,"other_count_stop","カウント増加を停止","OnYuukaToggleHarassmentCount");
    r=yuuka_replace_menu_item(r,lenp,"other_count_resume","カウント増加を再開","OnYuukaToggleHarassmentCount");
  }
  return r;
}
