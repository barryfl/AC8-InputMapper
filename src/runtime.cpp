// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
// Explicit compatibility component. The observational TM bridge is unrelated.
#define AC8_COMPAT
#include "directinput.cpp"
#include "version_guard.h"
#include <cwchar>
#include <cstdlib>
#include <initializer_list>

// No profile parser hook, engine calls, assembly thunks or profile mutation.
namespace profile {
void begin(void*,REFGUID,void*) {}
void released(void*,void*,void*,ULONG) {}
void acquired(void*,void*,HRESULT) {}
}
namespace compat {
struct Binding { int index=-1; bool inverted=false; int source=-1; }; // button index or axis index, zero based
struct Route { const wchar_t* name; int role; int output; bool axis; bool nativeInvert; };
// Negative outputs -1..-4 denote POV1 Up/Right/Down/Left.
constexpr Route routes[]={
    {L"Gun",1,0,false,false},{L"MSL",1,1,false,false},{L"Weapon",1,2,false,false},
    {L"Target",1,13,false,false},{L"View",1,4,false,false},
    {L"DPadUp",1,-1,false,false},{L"DPadRight",1,-2,false,false},
    {L"DPadDown",1,-3,false,false},{L"DPadLeft",1,-4,false,false},
    {L"Pitch",1,1,true,true},{L"Roll",1,0,true,false},{L"Yaw",1,5,true,false},
    {L"CameraPitch",1,4,true,false},{L"CameraYaw",1,3,true,false},
    {L"Radar",2,4,false,false},{L"Flare",2,0,false,false},
    {L"AutoPilot",2,2,false,false},{L"Pause",2,24,false,false},
    {L"Throttle",2,0,true,true}
};
constexpr size_t routeCount=sizeof(routes)/sizeof(routes[0]);
constexpr int maxSources=16; // two game-facing anchors plus fourteen private sources
struct Source {wchar_t name[33]{}; GUID instance{}; bool used=false;};
Source sources[maxSources]; int sourceCount=2;
Binding bindings[routeCount]; GUID instances[2]; bool active;bool hasAdditionalSources;
void stopExtras();
int namedSource(const wchar_t* name) {
    for(int i=0;i<sourceCount;++i) if(!_wcsicmp(name,sources[i].name)) return i;
    return -1;
}
bool sourceName(const wchar_t* name) {
    size_t n=wcslen(name);if(!n||n>32) return false;
    for(size_t i=0;i<n;++i) if(!((name[i]>=L'A'&&name[i]<=L'Z')||(name[i]>=L'a'&&name[i]<=L'z')||
        (name[i]>=L'0'&&name[i]<=L'9')||name[i]==L'_')) return false;
    return true;
}
unsigned char* image;
const wchar_t* axisNames[]={L"X",L"Y",L"Z",L"Rx",L"Ry",L"Rz",L"Slider1",L"Slider2"};
const GUID* axisGuids[]={&GUID_XAxis,&GUID_YAxis,&GUID_ZAxis,&GUID_RxAxis,&GUID_RyAxis,&GUID_RzAxis,&GUID_Slider,&GUID_Slider};
bool enabled() {return active;}
bool caller(void* p,DWORD rva) {return active&&image&&reinterpret_cast<ULONG_PTR>(p)==reinterpret_cast<ULONG_PTR>(image)+rva;}
int role(REFGUID g) {if(!active) return 0; for(int i=0;i<2;++i) if(g==instances[i]) return i+1; return 0;}
unsigned axisId(REFGUID g) {for(unsigned i=0;i<8;++i) if(g==*axisGuids[i]) return i; return 8;}
bool parse(const wchar_t* text,bool axis,Binding& b) {
    b=Binding{};
    if(!*text||!_wcsicmp(text,L"None")) return true;
    if(axis) {
        if(*text==L'-') {b.inverted=true;++text;}
        // First prototype: named rotational/translational axes only. Slider
        // object instance IDs are driver dependent; do not guess their order.
        for(int i=0;i<6;++i) if(!_wcsicmp(text,axisNames[i])) {b.index=i;return true;}
    } else if(!wcsncmp(text,L"Button",6)&&text[6]>=L'1'&&text[6]<=L'9') {
        const wchar_t* p=text+6; unsigned n=0;
        while(*p>=L'0'&&*p<=L'9') {n=n*10+(*p++-L'0');if(n>128) return false;}
        if(!*p&&n) {b.index=static_cast<int>(n)-1;return true;}
    }
    return false;
}
bool load(const wchar_t* path) {
    stopExtras();gameWindow(nullptr);active=false;hasAdditionalSources=false;for(auto& binding:bindings) binding=Binding{};
    for(auto& source:sources) source=Source{};
    sourceCount=2;wcscpy_s(sources[0].name,L"Stick");wcscpy_s(sources[1].name,L"Throttle");
    wchar_t text[128];
    GetPrivateProfileStringW(L"Compatibility",L"Enabled",L"0",text,128,path);
    if(wcscmp(text,L"1")) {obs::log("Compatibility disabled reason=Enabled_not_1 ini=%ls",path);return false;}
    GetPrivateProfileStringW(L"Compatibility",L"Version",L"",text,128,path);
    bool v2=!wcscmp(text,L"2");
    if(!v2&&wcscmp(text,L"1")) {obs::log("Compatibility rejected reason=INI_version");return false;}
    wchar_t sections[4096];DWORD n=GetPrivateProfileSectionNamesW(sections,4096,path);
    if(n>=4094) return false;
    // Resolve device declarations before bindings, regardless of section order.
    for(const wchar_t* section=sections;*section;section+=wcslen(section)+1) {
        bool anchor=!_wcsicmp(section,L"Stick")||!_wcsicmp(section,L"Throttle");
        bool options=!_wcsicmp(section,L"Compatibility");
        bool anchorBindings=!_wcsicmp(section,L"Stick.Bindings")||!_wcsicmp(section,L"Throttle.Bindings");
        bool extra=v2&&!_wcsnicmp(section,L"Device.",7);
        size_t length=wcslen(section);
        bool extraBindings=extra&&length>16&&!_wcsicmp(section+length-9,L".Bindings");
        if(!anchor&&!options&&!anchorBindings&&!extra) {
            obs::log("Compatibility rejected reason=unknown_section section=%ls",section);return false;
        }
        if(anchorBindings||extraBindings) continue;
        if(extra) {
            if(!sourceName(section+7)||namedSource(section+7)>=0||sourceCount==maxSources) {
                obs::log("Compatibility rejected reason=invalid_or_duplicate_device section=%ls",section);return false;
            }
            Source& source=sources[sourceCount++];wcscpy_s(source.name,section+7);
            if(!GetPrivateProfileStringW(section,L"InstanceGUID",L"",text,128,path)||
                FAILED(CLSIDFromString(text,&source.instance))||source.instance==GUID_NULL) return false;
        }
        wchar_t keys[4096];DWORD size=GetPrivateProfileStringW(section,nullptr,L"",keys,4096,path);
        if(size>=4094) return false;
        for(const wchar_t* key=keys;*key;key+=wcslen(key)+1)
            if(options?(_wcsicmp(key,L"Version")&&_wcsicmp(key,L"Enabled")):_wcsicmp(key,L"InstanceGUID")) {
                obs::log("Compatibility rejected reason=unknown_key section=%ls key=%ls",section,key);return false;
            }
    }
    for(int r=1;r<=2;++r) {
        const wchar_t* section=r==1?L"Stick":L"Throttle";
        if(!GetPrivateProfileStringW(section,L"InstanceGUID",L"",text,128,path)||
            FAILED(CLSIDFromString(text,&instances[r-1]))||instances[r-1]==GUID_NULL) {
            obs::log("Compatibility rejected reason=invalid_instance role=%d",r);return false;
        }
        sources[r-1].instance=instances[r-1];
    }
    for(int i=0;i<sourceCount;++i) for(int j=0;j<i;++j) if(sources[i].instance==sources[j].instance) {
        obs::log("Compatibility rejected reason=duplicate_device_instance");return false;
    }
    for(const wchar_t* section=sections;*section;section+=wcslen(section)+1) {
        int source=-1;
        if(!_wcsicmp(section,L"Stick.Bindings")) source=0;
        else if(!_wcsicmp(section,L"Throttle.Bindings")) source=1;
        else {
            size_t length=wcslen(section);
            if(v2&&!_wcsnicmp(section,L"Device.",7)&&length>16&&!_wcsicmp(section+length-9,L".Bindings")) {
                wchar_t name[128];if(length-16>=33) return false;
                wcsncpy_s(name,section+7,length-16);source=namedSource(name);
                if(source<2) {obs::log("Compatibility rejected reason=undeclared_binding_device section=%ls",section);return false;}
            }
        }
        if(source<0) continue;
        wchar_t entries[4096];DWORD size=GetPrivateProfileSectionW(section,entries,4096,path);
        if(size>=4094) return false;
        bool seen[routeCount]{};
        for(wchar_t* entry=entries;*entry;entry+=wcslen(entry)+1) {
            wchar_t* equals=wcschr(entry,L'=');if(!equals) return false;
            *equals=0;size_t i=0;
            for(;i<routeCount;++i) if(!_wcsicmp(entry,routes[i].name)&&(source>=2||routes[i].role==source+1)) break;
            *equals=L'=';Binding candidate;
            if(i==routeCount||seen[i]||!parse(equals+1,routes[i].axis,candidate)) {
                obs::log("Compatibility rejected reason=invalid_or_duplicate_binding section=%ls entry=%ls",section,entry);return false;
            }
            if(candidate.index>=0&&bindings[i].index>=0) {
                obs::log("Compatibility rejected reason=action_has_multiple_sources action=%ls",routes[i].name);return false;
            }
            candidate.source=source;
            // An explicit None in an old section must not erase an assignment
            // from another device when sections appear in the opposite order.
            if(bindings[i].index<0) bindings[i]=candidate;
            if(candidate.index>=0&&source>=2) {sources[source].used=true;hasAdditionalSources=true;}
            seen[i]=true;
        }
    }
    for(size_t i=0;i<routeCount;++i) for(size_t j=0;j<i;++j)
        if(routes[i].role==routes[j].role&&routes[i].axis&&routes[j].axis&&
            bindings[i].source==routes[i].role-1&&bindings[j].source==bindings[i].source&&
            bindings[i].index>=0&&bindings[i].index==bindings[j].index) {
            obs::log("Compatibility rejected reason=axis_source_reused role=%d",routes[i].role);return false;
        }
    return true;
}
bool supported(int r,unsigned axes,unsigned buttons,const unsigned char* present) {
    for(size_t i=0;i<routeCount;++i) if(routes[i].role==r&&bindings[i].index>=0&&bindings[i].source==r-1) {
        unsigned index=static_cast<unsigned>(bindings[i].index);
        if(routes[i].axis ? !(axes&(1u<<index)) : index>=buttons||(present&&!present[index])) return false;
    }
    return true;
}
bool version(HMODULE exe) {
    auto base=reinterpret_cast<unsigned char*>(exe); IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if(!obs::copy(&dos,base,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>0x100000||
       !obs::copy(&nt,base+dos.e_lfanew,sizeof(nt))||nt.Signature!=IMAGE_NT_SIGNATURE||
       nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.SizeOfImage!=profileImageSize) return false;
    unsigned char bytes[256];
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return false;
    bool matches=true;
    for(const auto& s:profileFingerprints) {
        unsigned char digest[32];
        if(s.length>sizeof(bytes)||!obs::copy(bytes,base+s.rva,s.length)||
           BCryptHash(algorithm,nullptr,0,bytes,static_cast<ULONG>(s.length),digest,sizeof(digest))<0||
           memcmp(digest,s.digest,sizeof(digest))) {matches=false;break;}
    }
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(!matches) return false;
    // Public COM call sites used to select the precise native input instances.
    const DWORD sites[]={0x4E6969E,0x4E696B9,0x4E699CB,0x4E69A02};
    const unsigned char slots[]={0x20,0x58,0x48,0x48};
    for(unsigned i=0;i<4;++i)
        if(!obs::copy(bytes,base+sites[i],3)||bytes[0]!=0xFF||bytes[1]!=0x50||bytes[2]!=slots[i]) return false;
    image=base; return true;
}
void initialize(HMODULE module) {
    wchar_t path[32768]; DWORD n=GetModuleFileNameW(module,path,32768);
    if(!n||n>=32768) return;
    wchar_t* slash=wcsrchr(path,L'\\'); if(!slash) return;
    wcscpy_s(slash+1,32768-static_cast<size_t>(slash+1-path),L"AC8InputMapper.ini");
    if(!load(path)) return;
    if(!version(GetModuleHandleW(nullptr))) {obs::log("Compatibility rejected reason=unsupported_executable ini=%ls",path);return;}
    active=true;
    obs::log("Compatibility version=2 build=0.2.0-experimental enabled=1 ini=%ls identity_scope=configured_devices_in_this_process state_scope=verified_game_callers",path);
    for(int r=1;r<=2;++r) {char g[40];obs::guidText(instances[r-1],g);obs::log("Compatibility configured role=%d instance=%s",r,g);}
    for(int i=2;i<sourceCount;++i) {char g[40];obs::guidText(sources[i].instance,g);obs::log("AdditionalSource name=%ls instance=%s used=%d",sources[i].name,g,sources[i].used);}
    for(size_t i=0;i<routeCount;++i) obs::log("Binding action=%ls role=%d kind=%s source_index_zero_based=%d inverted=%d virtual_channel=%d native_invert=%d source=%ls",
        routes[i].name,routes[i].role,routes[i].axis?"axis":"button",bindings[i].index,bindings[i].inverted,routes[i].output,routes[i].nativeInvert,sources[bindings[i].source>=0?bindings[i].source:routes[i].role-1].name);
}
GUID product(int r) {return GUID{r==1?0x22210738u:0xA2210738u,0,0,{0,0,0x50,0x49,0x44,0x56,0x49,0x44}};}
void identity(DIDEVICEINSTANCEW& d) {
    int r=role(d.guidInstance); if(!r) return;
    d.guidProduct=product(r); const wchar_t* name=r==1?L"X56 H.O.T.A.S. Stick (AC8 compatibility)":L"X56 H.O.T.A.S. Throttle (AC8 compatibility)";
    wcscpy_s(d.tszProductName,name);wcscpy_s(d.tszInstanceName,name);
    obs::log("Compatibility identity role=%d product=%08lX",r,d.guidProduct.Data1);
}
void identity(DIDEVICEINSTANCEA& d) {
    int r=role(d.guidInstance); if(!r) return;
    d.guidProduct=product(r); const char* name=r==1?"X56 H.O.T.A.S. Stick (AC8 compatibility)":"X56 H.O.T.A.S. Throttle (AC8 compatibility)";
    strcpy_s(d.tszProductName,name);strcpy_s(d.tszInstanceName,name);
    obs::log("Compatibility identity role=%d product=%08lX",r,d.guidProduct.Data1);
}
template<class T> void object(int r,T& d,DWORD* advertised) {
    if(!(d.dwType&DIDFT_AXIS)) return;
    unsigned source=axisId(d.guidType);
    for(size_t i=0;i<routeCount;++i) if(routes[i].role==r&&routes[i].axis&&bindings[i].source==r-1&&bindings[i].index==static_cast<int>(source)) {
        d.guidType=*axisGuids[routes[i].output]; d.dwOfs=static_cast<DWORD>(routes[i].output*4);
        if(advertised) *advertised|=1u<<routes[i].output;
        return; // dwType remains the real object ID used by SetProperty(BYID).
    }
    // Advertise externally supplied axes on otherwise unused physical objects.
    // Keep the driver's real object ID so the game's BYID range setter succeeds.
    if(source<6&&advertised) for(size_t i=0;i<routeCount;++i) {
        if(routes[i].role==r&&routes[i].axis&&bindings[i].index>=0&&bindings[i].source>=2&&
           !(*advertised&(1u<<routes[i].output))) {
            d.guidType=*axisGuids[routes[i].output];d.dwOfs=routes[i].output*4;
            *advertised|=1u<<routes[i].output;return;
        }
    }
}
void objectIdentity(int r,DIDEVICEOBJECTINSTANCEW& d,DWORD* advertised) {object(r,d,advertised);}
void objectIdentity(int r,DIDEVICEOBJECTINSTANCEA& d,DWORD* advertised) {object(r,d,advertised);}
bool externalOutputsReady(int r,DWORD advertised) {
    for(size_t i=0;i<routeCount;++i) if(routes[i].role==r&&routes[i].axis&&bindings[i].index>=0&&
        bindings[i].source>=2&&!(advertised&(1u<<routes[i].output))) return false;
    return true;
}
bool additional(REFGUID guid) {
    if(active) for(int i=2;i<sourceCount;++i) if(guid==sources[i].instance) return true;
    return false;
}
LONG axis(const DIJOYSTATE2& j,int i) {
    LONG values[]={j.lX,j.lY,j.lZ,j.lRx,j.lRy,j.lRz,j.rglSlider[0],j.rglSlider[1]};
    LONG x=i>=0&&i<8?values[i]:0;return x<-1000?-1000:x>1000?1000:x;
}
void translateSources(int r,const DIJOYSTATE2& in,const DIJOYSTATE2* extra,const bool* valid,DIJOYSTATE2& out) {
    out=DIJOYSTATE2{}; for(auto& p:out.rgdwPOV) p=0xFFFFFFFF;
    bool hats[4]{}; LONG axes[8]{};
    for(size_t i=0;i<routeCount;++i) if(routes[i].role==r&&bindings[i].index>=0) {
        const auto& route=routes[i]; const auto& b=bindings[i];
        DIJOYSTATE2 neutral{};
        const DIJOYSTATE2& state=b.source<2?in:(extra&&valid&&valid[b.source]?extra[b.source]:neutral);
        if(route.axis) {LONG x=axis(state,b.index);if(b.inverted!=route.nativeInvert) x=-x;axes[route.output]=x;}
        else {bool down=(state.rgbButtons[b.index]&0x80)!=0;
            if(route.output>=0) out.rgbButtons[route.output]=down?0x80:0;
            else hats[-route.output-1]=down;
        }
    }
    out.lX=axes[0];out.lY=axes[1];out.lZ=axes[2];out.lRx=axes[3];out.lRy=axes[4];out.lRz=axes[5];
    out.rglSlider[0]=axes[6];out.rglSlider[1]=axes[7];
    int x=static_cast<int>(hats[1])-static_cast<int>(hats[3]);
    int y=static_cast<int>(hats[2])-static_cast<int>(hats[0]);
    if(x||y) out.rgdwPOV[0]=y<0?(x<0?31500:x>0?4500:0):y>0?(x<0?22500:x>0?13500:18000):(x<0?27000:9000);
}
void translate(int r,const DIJOYSTATE2& in,DIJOYSTATE2& out) {translateSources(r,in,nullptr,nullptr,out);}
#include "additional_sources.h"
SRWLOCK sourceLock=SRWLOCK_INIT; DIJOYSTATE2 lastSource[2]; bool seenSource[2]; LONGLONG sourceTime[2];
void sourceLog(int r,const DIJOYSTATE2& j) {
    LARGE_INTEGER q;QueryPerformanceCounter(&q);AcquireSRWLockExclusive(&sourceLock);int i=r-1;
    bool emit=!seenSource[i]||memcmp(&j,&lastSource[i],sizeof(j))||q.QuadPart-sourceTime[i]>obs::frequency.QuadPart*2;
    seenSource[i]=true;lastSource[i]=j;if(emit) sourceTime[i]=q.QuadPart;ReleaseSRWLockExclusive(&sourceLock);
    if(!emit) return;
    char buttons[1024]="";size_t used=0;
    for(unsigned b=0;b<128;++b) if(j.rgbButtons[b]&0x80) used+=sprintf_s(buttons+used,sizeof(buttons)-used,"%u,",b+1);
    obs::log("PhysicalState role=%d axes=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld pressed_buttons_one_based=%s",r,j.lX,j.lY,j.lZ,j.lRx,j.lRy,j.lRz,j.rglSlider[0],j.rglSlider[1],buttons);
}
}
