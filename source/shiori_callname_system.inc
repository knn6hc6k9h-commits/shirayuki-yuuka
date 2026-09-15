/* Owner-menu links and install/switch event helpers.
 *
 * Creator-editable files beside shiori.dll:
 *   owner_menu_links.txt  -> SSP right-click Recommend / Portal links
 *   system_dialogue.txt   -> install / ghost-switch lines
 */
#define YUUKA_OWNER_FILE_MAX 16384
#define YUUKA_SYS_TEXT_MAX 512

static int yuuka_read_text_file(const char* filename,char* out,int cap){
  char path[1200];HANDLE f;DWORD got=0;
  if(!filename||!out||cap<2)return 0;
  out[0]=0;
  build_path(path,(int)sizeof(path),g_dir,filename);
  f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
  if(f==INVALID_HANDLE_VALUE)return 0;
  if(!ReadFile(f,out,(DWORD)(cap-1),&got,0))got=0;
  CloseHandle(f);
  if((int)got>=cap)got=(DWORD)(cap-1);
  out[got]=0;
  return got>0;
}

static int yuuka_ascii_equal_n(const char* a,int an,const char* b){
  int i,bn=slen(b);
  if(!a||!b||an!=bn)return 0;
  for(i=0;i<an;++i)if(a[i]!=b[i])return 0;
  return 1;
}

static void yuuka_copy_trimmed(const char* s,int n,char* out,int cap){
  int i=0,p=0;
  if(!out||cap<1)return;
  out[0]=0;
  while(i<n&&(s[i]==' '||s[i]=='\t'))++i;
  while(n>i&&(s[n-1]==' '||s[n-1]=='\t'))--n;
  while(i<n&&p<cap-1){
    unsigned char c=(unsigned char)s[i++];
    if(c<0x20||c=='\\')continue;
    out[p++]=(char)c;
  }
  out[p]=0;
}

static int yuuka_load_system_text(const char* key,const char* fallback,char* out,int cap){
  char buf[YUUKA_OWNER_FILE_MAX+1];
  int i=0,start,end,k=slen(key),p;
  if(!out||cap<1)return 0;
  out[0]=0;
  if(yuuka_read_text_file("system_dialogue.txt",buf,(int)sizeof(buf))){
    while(buf[i]){
      start=i;while(buf[i]&&buf[i]!='\r'&&buf[i]!='\n')++i;end=i;
      while(buf[i]=='\r'||buf[i]=='\n')++i;
      p=start;while(p<end&&(buf[p]==' '||buf[p]=='\t'))++p;
      if(p>=end||buf[p]=='#'||buf[p]==';')continue;
      if(end-p>k&&yuuka_ascii_equal_n(buf+p,k,key)&&buf[p+k]=='='){
        yuuka_copy_trimmed(buf+p+k+1,end-(p+k+1),out,cap);
        if(out[0])return 1;
      }
    }
  }
  if(fallback)yuuka_copy_trimmed(fallback,slen(fallback),out,cap);
  return out[0]!=0;
}

static HGLOBAL yuuka_owner_links_response(const char* section,long* outlen){
  char buf[YUUKA_OWNER_FILE_MAX+1],value[YUUKA_OWNER_FILE_MAX+1];
  int i=0,start,end,p,q,active=0,pos=0,count=0;
  if(!section||!yuuka_read_text_file("owner_menu_links.txt",buf,(int)sizeof(buf)))return 0;
  value[0]=0;
  while(buf[i]){
    start=i;while(buf[i]&&buf[i]!='\r'&&buf[i]!='\n')++i;end=i;
    while(buf[i]=='\r'||buf[i]=='\n')++i;
    p=start;while(p<end&&(buf[p]==' '||buf[p]=='\t'))++p;
    while(end>p&&(buf[end-1]==' '||buf[end-1]=='\t'))--end;
    if(p>=end||buf[p]=='#'||buf[p]==';')continue;
    if(buf[p]=='['&&end-p>=3&&buf[end-1]==']'){
      active=yuuka_ascii_equal_n(buf+p+1,end-p-2,section);
      continue;
    }
    if(!active)continue;
    q=p;while(q<end&&buf[q]!='|')++q;
    if(q>=end)continue;
    {
      char label[256],url[1024];
      yuuka_copy_trimmed(buf+p,q-p,label,(int)sizeof(label));
      yuuka_copy_trimmed(buf+q+1,end-q-1,url,(int)sizeof(url));
      if(!label[0]||!url[0])continue;
      append_text(value,(int)sizeof(value),&pos,label);
      if(pos<(int)sizeof(value)-1)value[pos++]=1;
      value[pos]=0;
      append_text(value,(int)sizeof(value),&pos,url);
      if(pos<(int)sizeof(value)-1)value[pos++]=1;
      if(pos<(int)sizeof(value)-1)value[pos++]=1; /* banner path: empty */
      if(pos<(int)sizeof(value)-1)value[pos++]=1; /* choice script: empty */
      if(pos<(int)sizeof(value)-1)value[pos++]=2;
      value[pos]=0;
      ++count;
    }
  }
  if(!count)return 0;
  return make_response(value,outlen);
}

static int yuuka_segment_at(const char* list,int index,char* out,int cap){
  int i=0,cur=0,p=0;
  if(!list||!out||cap<1)return 0;out[0]=0;
  while(list[i]&&cur<index){if((unsigned char)list[i]==1)++cur;++i;}
  if(cur!=index)return 0;
  while(list[i]&&(unsigned char)list[i]!=1&&p<cap-1)out[p++]=list[i++];
  out[p]=0;return p>0;
}

static int yuuka_type_is_ghost(const char* s){
  static const char g[]="ghost";int i;
  if(!s)return 0;
  for(i=0;g[i];++i)if(s[i]!=g[i])return 0;
  return s[i]==0||s[i]==' ';
}

static int yuuka_installed_ghost_name(const char* req,int n,int ex,char* out,int cap){
  char types[2048],names[4096],type[256],name[1024];int idx=0;
  if(!req||!out||cap<2)return 0;out[0]=0;types[0]=0;names[0]=0;
  get_header_value(req,n,"Reference0:",types,(int)sizeof(types));
  get_header_value(req,n,"Reference1:",names,(int)sizeof(names));
  if(!types[0]||!names[0])return 0;
  if(!ex){
    if(!yuuka_type_is_ghost(types))return 0;
    yuuka_copy_trimmed(names,slen(names),out,cap);return out[0]!=0;
  }
  for(idx=0;idx<64;++idx){
    if(!yuuka_segment_at(types,idx,type,(int)sizeof(type)))break;
    if(yuuka_type_is_ghost(type)&&yuuka_segment_at(names,idx,name,(int)sizeof(name))){
      yuuka_copy_trimmed(name,slen(name),out,cap);return out[0]!=0;
    }
  }
  return 0;
}

static HGLOBAL yuuka_install_complete_response(const char* req,int n,int ex,long* outlen){
  char ghost[1024],arrived[YUUKA_SYS_TEXT_MAX],question[YUUKA_SYS_TEXT_MAX],script[4096];int pos=0;
  if(!yuuka_installed_ghost_name(req,n,ex,ghost,(int)sizeof(ghost)))return 0;
  yuuka_load_system_text("install_arrived","さんがやってきました！",arrived,(int)sizeof(arrived));
  yuuka_load_system_text("install_question","私は席を外しますか？",question,(int)sizeof(question));
  script[0]=0;
  append_text(script,(int)sizeof(script),&pos,"\\0\\s[1]");
  append_text(script,(int)sizeof(script),&pos,ghost);
  append_text(script,(int)sizeof(script),&pos,arrived);
  append_text(script,(int)sizeof(script),&pos,"\\w5\\n");
  append_text(script,(int)sizeof(script),&pos,question);
  append_text(script,(int)sizeof(script),&pos,"\\n\\q[はい,OnYuukaInstallSwitchYes]\\q[いいえ,OnYuukaInstallSwitchNo]\\e");
  return make_response(script,outlen);
}

static HGLOBAL yuuka_handle_system_request(HGLOBAL original,long* outlen,int inlen){
  const char* req=(const char*)original;char text[YUUKA_SYS_TEXT_MAX],script[2048];int pos=0;
  HGLOBAL r;
  if(!original)return 0;

  if(request_has_id(req,inlen,"sakura.portalsites")){
    r=yuuka_owner_links_response("portal",outlen);if(r)return r;
  }
  if(request_has_id(req,inlen,"sakura.recommendsites")){
    r=yuuka_owner_links_response("recommend",outlen);if(r)return r;
  }

  if(request_has_id(req,inlen,"OnInstallBegin")){
    yuuka_load_system_text("install_begin","あて？誰か来たみたいですね？",text,(int)sizeof(text));
    script[0]=0;append_text(script,(int)sizeof(script),&pos,"\\0\\s[3]");append_text(script,(int)sizeof(script),&pos,text);append_text(script,(int)sizeof(script),&pos,"\\e");
    return make_response(script,outlen);
  }
  if(request_has_id(req,inlen,"OnInstallCompleteEx")){
    r=yuuka_install_complete_response(req,inlen,1,outlen);if(r)return r;
  }
  if(request_has_id(req,inlen,"OnInstallComplete")){
    r=yuuka_install_complete_response(req,inlen,0,outlen);if(r)return r;
  }
  if(request_has_id(req,inlen,"OnYuukaInstallSwitchYes")){
    yuuka_load_system_text("install_yes","分かりました♪では何かあればまた呼んでくれたら嬉しいです！",text,(int)sizeof(text));
    script[0]=0;append_text(script,(int)sizeof(script),&pos,"\\0\\s[1]");append_text(script,(int)sizeof(script),&pos,text);append_text(script,(int)sizeof(script),&pos,"\\w5\\![change,ghost,lastinstalled,--option=raise-event]\\e");
    return make_response(script,outlen);
  }
  if(request_has_id(req,inlen,"OnYuukaInstallSwitchNo")){
    yuuka_load_system_text("install_no","それでしたら、もう少しここにいさせてください♪",text,(int)sizeof(text));
    script[0]=0;append_text(script,(int)sizeof(script),&pos,"\\0\\s[1]");append_text(script,(int)sizeof(script),&pos,text);append_text(script,(int)sizeof(script),&pos,"\\e");
    return make_response(script,outlen);
  }
  if(request_has_id(req,inlen,"OnGhostChanging")){
    yuuka_load_system_text("ghost_changing","それでは失礼します♪",text,(int)sizeof(text));
    script[0]=0;append_text(script,(int)sizeof(script),&pos,"\\0\\s[1]");append_text(script,(int)sizeof(script),&pos,text);append_text(script,(int)sizeof(script),&pos,"\\e");
    return make_response(script,outlen);
  }
  return 0;
}
