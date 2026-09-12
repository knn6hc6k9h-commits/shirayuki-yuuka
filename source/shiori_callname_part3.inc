
static HGLOBAL callname_settings_menu(long* outlen){
  char b[2400];int p=0;load_callname();b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]呼び方を決めるんですね♪\\n\\n現在：");
  if(g_call_enabled){append_text(b,sizeof(b),&p,"「");append_text(b,sizeof(b),&p,g_call_name);append_text(b,sizeof(b),&p,"」\\n\\n");}
  else if(g_call_base[0]){append_text(b,sizeof(b),&p,"標準（");append_text(b,sizeof(b),&p,g_call_base);append_text(b,sizeof(b),&p,"さん）\\n\\n");}
  else append_text(b,sizeof(b),&p,"標準（登録名＋さん）\\n\\n");

  append_text(b,sizeof(b),&p,"\\q[「さん」で呼ぶ,OnYuukaCallNameSan]\\n");
  append_text(b,sizeof(b),&p,"\\q[「くん」で呼ぶ,OnYuukaCallNameKun]\\n");
  append_text(b,sizeof(b),&p,"\\q[「ちゃん」で呼ぶ,OnYuukaCallNameChan]\\n");
  append_text(b,sizeof(b),&p,"\\q[呼び捨てにする,OnYuukaCallNameNone]\\n");
  append_text(b,sizeof(b),&p,"\\q[自由な呼び名にする,OnYuukaCallNameCustom]\\n");
  append_text(b,sizeof(b),&p,"\\q[標準（登録名＋さん）に戻す,OnYuukaCallNameDefault]\\n\\n");
  append_text(b,sizeof(b),&p,"\\q[メインメニューに戻る,MainMenu]\\e");
  return make_response(b,outlen);
}

static HGLOBAL base_name_prompt(const char* suffix_label,const char* input_event,long* outlen){
  char b[1600];int p=0;b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]");append_text(b,sizeof(b),&p,suffix_label);
  append_text(b,sizeof(b),&p,"ですね♪\\n\\nまず、今ゆうかに登録しているお名前部分を入力してください。\\nたとえば『○○さん』なら『○○』と入力してください。\\n\\n\\![open,inputbox,");
  append_text(b,sizeof(b),&p,input_event);append_text(b,sizeof(b),&p,",--timeout=-1,--limit=20]\\e");
  return make_response(b,outlen);
}

static HGLOBAL callname_saved_response(long* outlen){
  char b[1200];int p=0;b[0]=0;
  append_text(b,sizeof(b),&p,"\\0\\s[1]はいっ♪\\w5これからは「");append_text(b,sizeof(b),&p,g_call_name);
  append_text(b,sizeof(b),&p,"」ってお呼びしますね。\\w5えへへ、よろしくお願いします♪\\n\\q[呼び方をもう一度決める,OnYuukaCallNameSettings]\\n\\q[メインメニューに戻る,MainMenu]\\e");
  return make_response(b,outlen);
}

static HGLOBAL save_suffix_from_input(const char* req,int n,const char* suffix,long* outlen){
  char raw[160],base[CALL_BASE_MAX+1],call[CALL_NAME_MAX+1];raw[0]=base[0]=call[0]=0;
  get_header_value(req,n,"Reference0:",raw,(int)sizeof(raw));
  if(!clean_input(raw,base,(int)sizeof(base)))
    return make_response("\\0\\s[2]あの……お名前をうまく読み取れませんでした。\\w5もう一度お願いします。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  if(!make_name_with_suffix(base,suffix,call,(int)sizeof(call))||!save_callname_record(base,call))
    return make_response("\\0\\s[2]ご、ごめんなさい……呼び方を保存できませんでした。\\w5もう一度試してみてください。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  return callname_saved_response(outlen);
}

static HGLOBAL choose_suffix(const char* suffix,const char* label,const char* input_event,long* outlen){
  char call[CALL_NAME_MAX+1];load_callname();
  if(!g_call_base[0])return base_name_prompt(label,input_event,outlen);
  if(!make_name_with_suffix(g_call_base,suffix,call,(int)sizeof(call))||!save_callname_record(g_call_base,call))
    return make_response("\\0\\s[2]ご、ごめんなさい……呼び方を保存できませんでした。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  return callname_saved_response(outlen);
}

static HGLOBAL custom_callname_start(long* outlen){
