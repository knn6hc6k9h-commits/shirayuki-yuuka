  load_callname();
  if(g_call_base[0]){
    int i=0;while(g_call_base[i]&&i<CALL_BASE_MAX){g_pending_base[i]=g_call_base[i];++i;}g_pending_base[i]=0;
    return make_response("\\0\\s[1]自由な呼び方ですね♪\\n\\n私に呼んでほしい呼び方を、そのまま入力してください。\\nたとえば『先輩』『先生』『○○先輩』みたいな呼び方でも大丈夫です♪\\n\\n\\![open,inputbox,OnYuukaCallNameCustomInput,--timeout=-1,--limit=30]\\e",outlen);
  }
  return make_response("\\0\\s[1]自由な呼び方ですね♪\\n\\nまず、今ゆうかに登録しているお名前部分を入力してください。\\nたとえば『○○さん』なら『○○』です。\\n\\n\\![open,inputbox,OnYuukaCallNameCustomBaseInput,--timeout=-1,--limit=20]\\e",outlen);
}

static HGLOBAL custom_base_input(const char* req,int n,long* outlen){
  char raw[160];raw[0]=0;g_pending_base[0]=0;get_header_value(req,n,"Reference0:",raw,(int)sizeof(raw));
  if(!clean_input(raw,g_pending_base,(int)sizeof(g_pending_base)))
    return make_response("\\0\\s[2]お名前をうまく読み取れませんでした……。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  return make_response("\\0\\s[1]はいっ。\\w5では次に、私に呼んでほしい呼び方をそのまま入力してください♪\\n『先輩』『先生』『○○先輩』みたいな呼び方でも大丈夫です。\\n\\n\\![open,inputbox,OnYuukaCallNameCustomInput,--timeout=-1,--limit=30]\\e",outlen);
}

static HGLOBAL custom_call_input(const char* req,int n,long* outlen){
  char raw[220],call[CALL_NAME_MAX+1];raw[0]=call[0]=0;
  if(!g_pending_base[0])return make_response("\\0\\s[2]ご、ごめんなさい……最初のお名前が分からなくなっちゃいました。\\nもう一度、最初から設定してください。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  get_header_value(req,n,"Reference0:",raw,(int)sizeof(raw));
  if(!clean_input(raw,call,(int)sizeof(call)))
    return make_response("\\0\\s[2]呼び方をうまく読み取れませんでした……。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  if(!save_callname_record(g_pending_base,call))
    return make_response("\\0\\s[2]ご、ごめんなさい……呼び方を保存できませんでした。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  g_pending_base[0]=0;return callname_saved_response(outlen);
}

static HGLOBAL default_callname_response(long* outlen){
  if(!reset_callname_to_default())
    return make_response("\\0\\s[2]ご、ごめんなさい……設定を保存できませんでした。\\n\\q[呼び方を決める画面に戻る,OnYuukaCallNameSettings]\\e",outlen);
  return make_response("\\0\\s[1]はいっ♪\\w5呼び方を標準の『登録名＋さん』に戻しました。\\w5また変えたくなったら『呼び方を決める』からいつでも変えられますよ。\\n\\q[呼び方をもう一度決める,OnYuukaCallNameSettings]\\n\\q[メインメニューに戻る,MainMenu]\\e",outlen);
}

static HGLOBAL handle_callname_event(HGLOBAL h,long* lenp,int inlen){
  const char* req=(const char*)h;HGLOBAL r=0;long rn=0;
  if(request_has_id(req,inlen,"OnYuukaCallNameSettings"))r=callname_settings_menu(&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameSan"))r=choose_suffix("さん","『さん』付け","OnYuukaCallNameSanInput",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameKun"))r=choose_suffix("くん","『くん』付け","OnYuukaCallNameKunInput",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameChan"))r=choose_suffix("ちゃん","『ちゃん』付け","OnYuukaCallNameChanInput",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameNone"))r=choose_suffix("","呼び捨て","OnYuukaCallNameNoneInput",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameSanInput"))r=save_suffix_from_input(req,inlen,"さん",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameKunInput"))r=save_suffix_from_input(req,inlen,"くん",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameChanInput"))r=save_suffix_from_input(req,inlen,"ちゃん",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameNoneInput"))r=save_suffix_from_input(req,inlen,"",&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameCustom"))r=custom_callname_start(&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameCustomBaseInput"))r=custom_base_input(req,inlen,&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameCustomInput"))r=custom_call_input(req,inlen,&rn);
  else if(request_has_id(req,inlen,"OnYuukaCallNameDefault"))r=default_callname_response(&rn);
  if(r){GlobalFree(h);if(lenp)*lenp=rn;return r;}return 0;
}
