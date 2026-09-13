/*
 * Editable word-to-meaning dictionary.
 *
 * word_meanings.txt lives beside shiori.dll and is a local user setting.
 * Sections map to event IDs by upper-casing the first ASCII character:
 *   [apology] -> Apology
 *   [thank]   -> Thank
 * One word or phrase per line; the first matching section wins.
 *
 * Apology is special: it is routed through the core's existing apology path
 * so the existing mood response and secret-count reduction remain intact.
 */
#define YUUKA_MEANING_FILE_MAX 16384
#define YUUKA_MEANING_EVENT_MAX 64
#define YUUKA_MEANING_INPUT_MAX 512

static const char yuuka_word_meanings_defaults[]=
"# 白雪ゆうか 単語の意味設定\r\n"
"#\r\n"
"# 「ゆうかに話しかける」で入力された文章を、意味ごとに分類する一覧です。\r\n"
"# [意味名] の下に、同じ意味として扱いたい単語・短い文章を1行ずつ書きます。\r\n"
"# 行を追加すると判定語を追加、行を削除すると判定語を削除できます。\r\n"
"# 上から順に調べ、最初に一致した意味が使われます。\r\n"
"#\r\n"
"# 意味名は対応するイベントIDになります。\r\n"
"# 例: [thank] -> EVENT|Thank|...\r\n"
"#     [praise] -> EVENT|Praise|...\r\n"
"# 新しい意味を作る場合は dialogue.txt または user_events.txt に\r\n"
"# 対応する EVENT を追加してください。\r\n"
"#\r\n"
"# [apology] は特別扱いです。\r\n"
"# ここに登録された言葉は既存の Apology 判定へ渡され、\r\n"
"# 秘密のカウントを減らす既存の動作もそのまま働きます。\r\n"
"#\r\n"
"# [thank] は、EVENT|Thank|... があればそれを優先します。\r\n"
"# Thankイベントがない場合は既存の「ありがとう」返答を使います。\r\n"
"#\r\n"
"# このファイルはネットワーク更新では上書きされません。\r\n"
"\r\n"
"[apology]\r\n"
"ごめん\r\n"
"ごめんなさい\r\n"
"すみません\r\n"
"すいません\r\n"
"申し訳\r\n"
"申し訳ない\r\n"
"申し訳ありません\r\n"
"悪かった\r\n"
"わるかった\r\n"
"謝る\r\n"
"謝って\r\n"
"\r\n"
"[thank]\r\n"
"ありがとう\r\n"
"ありがとうございます\r\n"
"ありがと\r\n"
"感謝\r\n"
"サンキュー\r\n"
"thank you\r\n"
"thanks\r\n";

static void yuuka_ensure_word_meanings(void){
  char path[1200];
  HANDLE f;
  DWORD wrote=0;
  build_path(path,(int)sizeof(path),g_dir,"word_meanings.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f!=INVALID_HANDLE_VALUE){CloseHandle(f);return;}
  f=CreateFileA(path,GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return;
  WriteFile(f,yuuka_word_meanings_defaults,(DWORD)slen(yuuka_word_meanings_defaults),&wrote,0);
  CloseHandle(f);
}

static unsigned char yuuka_ascii_fold(unsigned char c){
  if(c>='A'&&c<='Z')return (unsigned char)(c+('a'-'A'));
  return c;
}

static int yuuka_ascii_equal_ci(const char* a,const char* b){
  int i=0;
  if(!a||!b)return 0;
  while(a[i]&&b[i]){
    if(yuuka_ascii_fold((unsigned char)a[i])!=yuuka_ascii_fold((unsigned char)b[i]))return 0;
    ++i;
  }
  return a[i]==0&&b[i]==0;
}

static int yuuka_contains_ci_ascii(const char* hay,const char* needle){
  int i,j,n;
  if(!hay||!needle||!needle[0])return 0;
  n=slen(needle);
  for(i=0;hay[i];++i){
    for(j=0;j<n&&hay[i+j];++j){
      unsigned char a=(unsigned char)hay[i+j];
      unsigned char b=(unsigned char)needle[j];
      if(a<0x80&&b<0x80){a=yuuka_ascii_fold(a);b=yuuka_ascii_fold(b);}
      if(a!=b)break;
    }
    if(j==n)return 1;
  }
  return 0;
}

static int yuuka_section_to_event(const char* s,int n,char* out,int cap){
  int i,p=0;
  unsigned char c;
  if(!s||!out||cap<2||n<=0)return 0;
  while(n>0&&(s[0]==' '||s[0]=='\t')){++s;--n;}
  while(n>0&&(s[n-1]==' '||s[n-1]=='\t'))--n;
  if(n<=0||n>=cap)return 0;
  for(i=0;i<n;++i){
    c=(unsigned char)s[i];
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return 0;
  }
  if(!((s[0]>='a'&&s[0]<='z')||(s[0]>='A'&&s[0]<='Z')))return 0;
  c=(unsigned char)s[0];
  if(c>='a'&&c<='z')c=(unsigned char)(c-('a'-'A'));
  out[p++]=(char)c;
  for(i=1;i<n&&p<cap-1;++i)out[p++]=s[i];
  out[p]=0;
  return p>0;
}

static int yuuka_find_meaning(const char* input,char* event,int event_cap,int* loaded){
  char path[1200],buf[YUUKA_MEANING_FILE_MAX+1],section[YUUKA_MEANING_EVENT_MAX+1];
  HANDLE f;
  DWORD got=0;
  int i=0,start,end,v,vend;
  if(loaded)*loaded=0;
  if(event&&event_cap>0)event[0]=0;
  if(!input||!input[0]||!event||event_cap<2)return 0;

  yuuka_ensure_word_meanings();
  build_path(path,(int)sizeof(path),g_dir,"word_meanings.txt");
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!ReadFile(f,buf,YUUKA_MEANING_FILE_MAX,&got,0))got=0;
  CloseHandle(f);
  if(loaded)*loaded=1;
  if(!got)return 0;
  buf[got]=0;
  section[0]=0;

  while(i<(int)got){
    start=i;
    while(i<(int)got&&buf[i]!='\r'&&buf[i]!='\n')++i;
    end=i;
    while(i<(int)got&&(buf[i]=='\r'||buf[i]=='\n'))++i;

    v=start;
    while(v<end&&(buf[v]==' '||buf[v]=='\t'))++v;
    vend=end;
    while(vend>v&&(buf[vend-1]==' '||buf[vend-1]=='\t'))--vend;
    if(v>=vend||buf[v]=='#'||buf[v]==';')continue;

    if(buf[v]=='['&&vend-v>=3&&buf[vend-1]==']'){
      if(!yuuka_section_to_event(buf+v+1,vend-v-2,section,(int)sizeof(section)))section[0]=0;
      continue;
    }

    if(!section[0])continue;
    {
      char word[512];
      int n=vend-v,j;
      if(n<=0)continue;
      if(n>=(int)sizeof(word))n=(int)sizeof(word)-1;
      for(j=0;j<n;++j)word[j]=buf[v+j];
      word[n]=0;
      if(yuuka_contains_ci_ascii(input,word)){
        int k=0;
        while(section[k]&&k<event_cap-1){event[k]=section[k];++k;}
        event[k]=0;
        return 1;
      }
    }
  }
  return 0;
}

static HGLOBAL yuuka_meaning_call_event(const char* id,long* outlen){
  static const char a[]="GET SHIORI/3.0\r\nCharset: UTF-8\r\nSender: SSP\r\nSecurityLevel: local\r\nID: ";
  static const char e[]="\r\n\r\n";
  int na=(int)(sizeof(a)-1),ni=slen(id),ne=(int)(sizeof(e)-1),n=na+ni+ne;
  HGLOBAL h,r;
  long rn=n;
  if(!g_base_request||!id||!id[0])return 0;
  h=GlobalAlloc(GPTR,(SIZE_T)n+1);
  if(!h)return 0;
  copy_bytes((char*)h,a,na);
  copy_bytes((char*)h+na,id,ni);
  copy_bytes((char*)h+na+ni,e,ne);
  ((char*)h)[n]=0;
  r=g_base_request(h,&rn);
  if(outlen)*outlen=rn;
  return r;
}

static HGLOBAL yuuka_meaning_call_speak(const char* input,long* outlen){
  static const char a[]="GET SHIORI/3.0\r\nCharset: UTF-8\r\nSender: SSP\r\nSecurityLevel: local\r\nID: OnYuukaSpeakInput\r\nReference0: ";
  static const char e[]="\r\n\r\n";
  int na=(int)(sizeof(a)-1),ni=slen(input),ne=(int)(sizeof(e)-1),n=na+ni+ne;
  HGLOBAL h,r;
  long rn=n;
  if(!g_base_request||!input)return 0;
  h=GlobalAlloc(GPTR,(SIZE_T)n+1);
  if(!h)return 0;
  copy_bytes((char*)h,a,na);
  copy_bytes((char*)h+na,input,ni);
  copy_bytes((char*)h+na+ni,e,ne);
  ((char*)h)[n]=0;
  r=g_base_request(h,&rn);
  if(outlen)*outlen=rn;
  return r;
}

static int yuuka_response_has_value(HGLOBAL r,long n){
  const char* p=(const char*)r;
  static const char needle[]="\nValue: ";
  int i,j,k=(int)(sizeof(needle)-1);
  if(!p||n<=0)return 0;
  for(i=0;i+k<=n;++i){
    for(j=0;j<k&&p[i+j]==needle[j];++j){}
    if(j==k)return 1;
  }
  return 0;
}

static int yuuka_legacy_semantic_hint(const char* input){
  static const char* terms[]={
    "ごめ","すみま","すいま","申し訳","悪かった","わるかった","謝",
    "ありがとう",0
  };
  int i=0;
  while(terms[i]){
    if(yuuka_contains_ci_ascii(input,terms[i]))return 1;
    ++i;
  }
  return 0;
}

static void yuuka_obfuscate_semantic_input(const char* in,char* out,int cap){
  int i=0,p=0,n,k;
  unsigned char c;
  if(!out||cap<1)return;
  out[0]=0;
  while(in&&in[i]&&p<cap-1){
    c=(unsigned char)in[i];
    if(c<0x80)n=1;
    else if((c&0xE0)==0xC0)n=2;
    else if((c&0xF0)==0xE0)n=3;
    else if((c&0xF8)==0xF0)n=4;
    else n=1;
    for(k=0;k<n&&in[i]&&p<cap-1;++k)out[p++]=in[i++];
    if(p+3<cap){
      out[p++]=(char)0xE2;
      out[p++]=(char)0x80;
      out[p++]=(char)0x8B;
    }
  }
  out[p]=0;
}

static HGLOBAL yuuka_handle_meaning_input(HGLOBAL original,long* outlen,int inlen){
  char input[YUUKA_MEANING_INPUT_MAX+1],event[YUUKA_MEANING_EVENT_MAX+1];
  char masked[2048];
  HGLOBAL r;
  long rn=0;
  int loaded=0;

  if(!original||!request_has_id((const char*)original,inlen,"OnYuukaSpeakInput"))return 0;
  input[0]=0;
  get_header_value((const char*)original,inlen,"Reference0:",input,(int)sizeof(input));
  if(!input[0])return 0;

  if(yuuka_find_meaning(input,event,(int)sizeof(event),&loaded)){
    if(yuuka_ascii_equal_ci(event,"Apology")){
      r=yuuka_meaning_call_speak("ごめんなさい",&rn);
      if(r){if(outlen)*outlen=rn;return r;}
      return 0;
    }

    r=yuuka_meaning_call_event(event,&rn);
    if(r&&yuuka_response_has_value(r,rn)){
      if(outlen)*outlen=rn;
      return r;
    }
    if(r)GlobalFree(r);

    /* Thank works immediately with the existing REPLY|ありがとう entry.
       If a dedicated EVENT|Thank|... is added later, it takes priority. */
    if(yuuka_ascii_equal_ci(event,"Thank")){
      r=yuuka_meaning_call_speak("ありがとう",&rn);
      if(r){if(outlen)*outlen=rn;return r;}
    }
    return 0;
  }

  /*
   * The old core has built-in apology recognition and a legacy "ありがとう"
   * REPLY.  Once this editable dictionary exists, removing those words should
   * really remove their semantic classification.  Zero-width spaces keep the
   * visible text the same while preventing the old byte-substring matcher from
   * silently re-classifying a removed default word.
   */
  if(loaded&&yuuka_legacy_semantic_hint(input)){
    yuuka_obfuscate_semantic_input(input,masked,(int)sizeof(masked));
    r=yuuka_meaning_call_speak(masked,&rn);
    if(r){if(outlen)*outlen=rn;return r;}
  }

  return 0;
}
