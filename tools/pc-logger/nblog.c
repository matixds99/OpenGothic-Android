// NB PC logger: a plugin for Union (System\Autorun\nblog.dll) for Gothic II NoTR 2.6 (the Gothic2.exe with symbols).
// It hooks zCParser::DoStack(int pos) at 0x791960 (every call of a script function) and writes to nblog.txt next to the
// DLL the calls of the functions listed in nblog.cfg: time, name, SELF and OTHER (names of the npcs).
//   nblog.cfg:  SELF 3752 / OTHER 3753 (symbol indexes), MINMS 200 (the same function not more often than that),
//               then lines "pos name" (pos = the address of the function in the DAT)
// No CRT, no Windows headers: build with build.sh (clang + lld-link).
typedef unsigned int u32; typedef int i32; typedef unsigned char u8; typedef void* P;
#define WINAPI __attribute__((stdcall))
extern "C" {
extern WINAPI P   CreateFileA(const char*,u32,u32,P,u32,u32,P);
extern WINAPI i32 WriteFile(P,const void*,u32,u32*,P);
extern WINAPI i32 ReadFile(P,void*,u32,u32*,P);
extern WINAPI i32 CloseHandle(P);
extern WINAPI u32 GetFileSize(P,u32*);
extern WINAPI u32 GetTickCount(void);
extern WINAPI u32 GetModuleFileNameA(P,char*,u32);
extern WINAPI P   VirtualAlloc(P,u32,u32,u32);
extern WINAPI i32 VirtualProtect(P,u32,u32,u32*);
extern WINAPI i32 IsBadReadPtr(const void*,u32);
extern WINAPI P   CreateThread(P,u32,u32(WINAPI*)(P),P,u32,u32*);
extern WINAPI P   GetModuleHandleA(const char*);
extern WINAPI void Sleep(u32);
}

// NB's UNION_ABI.DLL refuses unknown plugins (name-hash whitelist) -> patch the check in memory:
// 'test esi,esi' (85 f6) -> 'xor esi,esi' (31 f6) at RVA 0x6ea63. Done by polling, as the file on disk is replaced by NB.
static u32 WINAPI patchThread(P){
  for(u32 k=0;k<200000;k++){
    u8* b=(u8*)GetModuleHandleA("UNION_ABI.DLL");
    if(b){
      u8* p=b+0x6ea63;
      if(p[0]==0x85 && p[1]==0xf6 && p[2]==0x0f && p[3]==0x84){
        u32 old; VirtualProtect(p,2,0x40,&old); p[0]=0x31; p[1]=0xf6; VirtualProtect(p,2,old,&old);
        }
      return 0;
      }
    Sleep(0);
    }
  return 0;
  }

#define HOOK_VA   0x791960u
#define GETSYMBOL 0x7938c0u   // zCPar_Symbol* zCParser::GetSymbol(int)
#define GETINSTADR 0x7a1dc0u  // void* zCPar_Symbol::GetInstanceAdr()
#define NPC_GETNAME 0x72f820u // zSTRING oCNpc::GetName(int)  (hidden return pointer)
static const u8 PROLOGUE[7] = {0x6a,0xff,0x68,0x80,0xa8,0x82,0x00};

typedef void (__attribute__((thiscall)) *DoStackFn)(P self, i32 pos);
typedef P    (__attribute__((thiscall)) *GetSymbolFn)(P self, i32 idx);
typedef P    (__attribute__((thiscall)) *GetInstFn)(P sym);
typedef P    (__attribute__((thiscall)) *GetNameFn)(P npc, P ret, i32 flag);

static DoStackFn orig;
static P   logf_;
static i32 symSelf=-1, symOther=-1, minMs=200;
#define MAXE 4096
static struct { i32 pos; char name[64]; u32 last; } ent[MAXE];
static i32 nEnt;
static u32 depth;

static u32 slen(const char* s){ u32 n=0; while(s[n]) ++n; return n; }
static char* put(char* o,const char* s){ while(*s) *o++=*s++; return o; }
static char* putn(char* o,u32 v){ char t[12]; int n=0; do{ t[n++]=(char)('0'+v%10); v/=10; }while(v); while(n) *o++=t[--n]; return o; }

static const char* npcName(P parser,i32 idx,char* buf){
  buf[0]=0;
  if(idx<0) return buf;
  P sym = ((GetSymbolFn)GETSYMBOL)(parser,idx);
  if(!sym) return buf;
  P npc = ((GetInstFn)GETINSTADR)(sym);
  if(!npc || IsBadReadPtr(npc,0x100)) return buf;
  u8 str[32]; for(int i=0;i<32;i++) str[i]=0;
  ((GetNameFn)NPC_GETNAME)(npc,str,0);
  const char* p = *(const char**)(str+8);       // zSTRING: vtbl, alloc, ptr, len, res
  if(!p || IsBadReadPtr(p,64)) return buf;
  u32 n=0; while(n<40 && p[n]) { buf[n]=p[n]; ++n; } buf[n]=0;
  return buf;
  }

static void write(const char* s,u32 n){ u32 w; WriteFile(logf_,s,n,&w,0); }

static void __attribute__((thiscall)) detour(P self, i32 pos){
  i32 hit=-1;
  for(i32 i=0;i<nEnt;i++) if(ent[i].pos==pos){ hit=i; break; }
  if(hit>=0 && logf_){
    u32 now=GetTickCount();
    if(now-ent[hit].last>=(u32)minMs){
      ent[hit].last=now;
      char line[256]; char a[48], b[48]; char* o=line;
      o=put(o,"["); o=putn(o,now); o=put(o,"] "); o=put(o,ent[hit].name);
      npcName(self,symSelf,a); npcName(self,symOther,b);
      if(a[0]){ o=put(o," self="); o=put(o,a); }
      if(b[0]){ o=put(o," other="); o=put(o,b); }
      *o++='\r'; *o++='\n';
      write(line,(u32)(o-line));
      }
    }
  orig(self,pos);
  }

static i32 parseInt(const char*& p){ i32 v=0; while(*p>='0'&&*p<='9') v=v*10+(*p++-'0'); return v; }

static void loadCfg(const char* path){
  P f=CreateFileA(path,0x80000000u,1,0,3,0x80,0);
  if(f==(P)-1) return;
  u32 sz=GetFileSize(f,0); if(sz>1000000) sz=1000000;
  static char buf[1000001]; u32 rd=0; ReadFile(f,buf,sz,&rd,0); CloseHandle(f); buf[rd]=0;
  for(char* p=buf; *p; ){
    char* e=p; while(*e && *e!='\n') ++e;
    char* nx = *e ? e+1 : e; *e=0; if(e>p && e[-1]=='\r') e[-1]=0;
    const char* q=p;
    if(q[0]=='S'&&q[1]=='E'&&q[2]=='L'&&q[3]=='F'){ q+=5; symSelf=parseInt(q); }
    else if(q[0]=='O'&&q[1]=='T'&&q[2]=='H'){ q+=6; symOther=parseInt(q); }
    else if(q[0]=='M'&&q[1]=='I'&&q[2]=='N'){ q+=6; minMs=parseInt(q); }
    else if(q[0]>='0'&&q[0]<='9' && nEnt<MAXE){
      ent[nEnt].pos=parseInt(q); while(*q==' ') ++q;
      u32 n=0; while(q[n] && n<63){ ent[nEnt].name[n]=q[n]; ++n; } ent[nEnt].name[n]=0; ent[nEnt].last=0; ++nEnt;
      }
    p=nx;
    }
  }


// ---- hooks inside NB's UNION_ABI.DLL (the teleport is done there, not in scripts) ----
// all four functions start with 5 position-independent bytes; checked before patching
typedef u32 (__attribute__((thiscall)) *EligFn)(P npc);
static EligFn origElig;
static u32 lastElig[64]; static P lastEligNpc[64];

static void hexs(char*& o,u32 v){ for(int s=28;s>=0;s-=4){ u32 d=(v>>s)&15; *o++=(char)(d<10?'0'+d:'a'+d-10);} }

static void wlog(const char* m){ u32 n=slen(m); write(m,n); write("\r\n",2); }
static void evt(const char* name,u32 ecx){
  if(!logf_) return;
  char line[160]; char* o=line;
  o=put(o,"["); o=putn(o,GetTickCount()); o=put(o,"] PLUGIN "); o=put(o,name); o=put(o," ecx="); hexs(o,ecx);
  *o++='\r'; *o++='\n'; write(line,(u32)(o-line));
  }
static const u8 PRO_T[6]={0x55,0x8b,0xec,0x83,0xec,0x54};   // push ebp; mov ebp,esp; sub esp,0x54
extern "C" void evtTeleport(u32 ecx){
  if(!logf_) return;
  char a[48]; a[0]=0; P npc=(P)ecx;
  if(!IsBadReadPtr(npc,0x100)){
    u8 str[32]; for(int i=0;i<32;i++) str[i]=0;
    ((GetNameFn)NPC_GETNAME)(npc,str,0);
    const char* p=*(const char**)(str+8);
    if(p && !IsBadReadPtr(p,64)){ u32 n=0; while(n<40 && p[n]){ a[n]=p[n]; ++n; } a[n]=0; }
    }
  char line[160]; char* o=line;
  o=put(o,"["); o=putn(o,GetTickCount()); o=put(o,"] PLUGIN TELEPORT self="); o=put(o,a);
  *o++='\r'; *o++='\n'; write(line,(u32)(o-line));
  }
extern "C" void evtSpecial11(u32 ecx){ evt("special_attack_11 (0x1007b050)",ecx); }
extern "C" void evtImarah(u32 ecx){ evt("imarah_special (0x1007f810)",ecx); }
extern "C" void evtPlace(u32 ecx){ evt("teleport_choose_place (0x102536b0)",ecx); }

static u32 __attribute__((thiscall)) eligDetour(P npc){
  u32 r=origElig(npc);
  if((r&0xff) && logf_){
    u32 now=GetTickCount(); int slot=-1;
    for(int i=0;i<64;i++) if(lastEligNpc[i]==npc){ slot=i; break; }
    if(slot<0){ for(int i=0;i<64;i++) if(!lastEligNpc[i]){ slot=i; lastEligNpc[i]=npc; lastElig[i]=0; break; } }
    if(slot>=0 && now-lastElig[slot]<3000) return r;
    if(slot>=0) lastElig[slot]=now;
    char a[48]; a[0]=0;
    if(!IsBadReadPtr(npc,0x100)){
      u8 str[32]; for(int i=0;i<32;i++) str[i]=0;
      ((GetNameFn)NPC_GETNAME)(npc,str,0);
      const char* p=*(const char**)(str+8);
      if(p && !IsBadReadPtr(p,64)){ u32 n=0; while(n<40 && p[n]){ a[n]=p[n]; ++n; } a[n]=0; }
      }
    char line[160]; char* o=line;
    o=put(o,"["); o=putn(o,now); o=put(o,"] PLUGIN teleport_eligible self="); o=put(o,a);
    *o++='\r'; *o++='\n'; write(line,(u32)(o-line));
    }
  return r;
  }

static const u8 PRO_A[5]={0x55,0x8b,0xec,0x6a,0xff};   // push ebp; mov ebp,esp; push -1
static const u8 PRO_E[5]={0x55,0x8b,0xec,0x51,0x53};   // push ebp; mov ebp,esp; push ecx; push ebx

static u8* hookAt(u8* target,const u8* pro,P fn,bool pre,int len=5){
  for(int i=0;i<len;i++) if(target[i]!=pro[i]) return 0;
  u8* tr=(u8*)VirtualAlloc(0,64,0x3000,0x40); if(!tr) return 0;
  u8* o=tr;
  if(pre){        // pushad; pushfd; push ecx; call fn; add esp,4; popfd; popad
    *o++=0x60; *o++=0x9c; *o++=0x51; *o++=0xE8; *(i32*)o=(i32)((u32)fn-((u32)o+4)); o+=4;
    *o++=0x83; *o++=0xC4; *o++=0x04; *o++=0x9D; *o++=0x61;
    }
  u8* stolen=o;
  for(int i=0;i<len;i++) *o++=target[i];
  *o++=0xE9; *(i32*)o=(i32)((u32)target+len-((u32)o+4));
  u32 old; VirtualProtect(target,len,0x40,&old);
  target[0]=0xE9;
  *(i32*)(target+1)=(i32)((pre? (u32)tr : (u32)fn)-((u32)target+5));
  for(int i=5;i<len;i++) target[i]=0x90;
  VirtualProtect(target,len,old,&old);
  return pre? tr : stolen;
  }

static u32 WINAPI pluginThread(P){
  for(u32 k=0;k<200000;k++){
    u8* b=(u8*)GetModuleHandleA("UNION_ABI.DLL");
    if(b){
      Sleep(3000);                      // let it finish loading
      u8* r;
      r=hookAt(b+0x2b1410,PRO_T,(P)evtTeleport,true,6);
      wlog(r?"nblog: hook TELEPORT ok":"nblog: TELEPORT - other plugin version, skipped");
      r=hookAt(b+0x2b1890,PRO_E,(P)eligDetour,false);
      if(r){ origElig=(EligFn)r; wlog("nblog: hook eligible ok"); }
      else wlog("nblog: eligible - other plugin version, skipped");
      return 0;
      }
    Sleep(0);
    }
  return 0;
  }

static void install(){
  u8* target=(u8*)HOOK_VA;
  for(int i=0;i<7;i++) if(target[i]!=PROLOGUE[i]) { write("nblog: unknown exe, hook skipped\r\n",34); return; }
  u8* tr=(u8*)VirtualAlloc(0,32,0x3000,0x40);
  if(!tr) return;
  for(int i=0;i<7;i++) tr[i]=target[i];
  tr[7]=0xE9; *(i32*)(tr+8)=(i32)((u32)target+7-((u32)tr+12));
  orig=(DoStackFn)tr;
  u32 old; VirtualProtect(target,7,0x40,&old);
  target[0]=0xE9; *(i32*)(target+1)=(i32)((u32)&detour-((u32)target+5)); target[5]=0x90; target[6]=0x90;
  VirtualProtect(target,7,old,&old);
  }

extern "C" i32 WINAPI DllMain(P h,u32 reason,P){
  if(reason==1){
    CreateThread(0,0,patchThread,0,0,0);
    char path[300]; u32 n=GetModuleFileNameA(h,path,260); while(n && path[n-1]!='\\') --n;
    char cfg[320], out[320]; for(u32 i=0;i<n;i++){ cfg[i]=out[i]=path[i]; }
    const char* c="nblog.cfg"; const char* t="nblog.txt";
    u32 i; for(i=0;c[i];i++) cfg[n+i]=c[i]; cfg[n+i]=0;
    for(i=0;t[i];i++) out[n+i]=t[i]; out[n+i]=0;
    loadCfg(cfg);
    if(nEnt==0) return 1;                    // no config: does nothing
    logf_=CreateFileA(out,0x00000004u,3,0,4,0x80,0);   // FILE_APPEND_DATA, OPEN_ALWAYS
    if(logf_==(P)-1){ logf_=0; return 1; }
    write("nblog: start\r\n",14);
    install();
    CreateThread(0,0,pluginThread,0,0,0);
    }
  return 1;
  }
