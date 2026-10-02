// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
#include "../src/runtime.cpp"
void require(bool value,const char* message) {if(!value) {printf("FAIL: %s\n",message);exit(1);}}
struct Fake {void** table;int kind;};
void* rootTable[11]{};void* pedalTable[32]{};void* boxTable[32]{};
Fake root{rootTable,0},pedals{pedalTable,1},box{boxTable,2};
int creates[3]{},reads[3]{},polls[3]{},acquires[3]{},unacquires[3]{},releases[3]{};
HRESULT readStatus[3]={S_OK,S_OK,S_OK},acquireStatus[3]={S_OK,S_OK,S_OK},pollStatus[3]={S_OK,S_OK,S_OK};
HRESULT formatStatus=S_OK;int lostReads=0;LONG pedalValue=750;
HRESULT STDMETHODCALLTYPE extraCreate(void*,REFGUID guid,void** output,IUnknown*) {
    int kind=guid.Data1==0x33333333?1:2;++creates[kind];*output=kind==1?&pedals:&box;return S_OK;
}
ULONG STDMETHODCALLTYPE extraRelease(void* object) {++releases[static_cast<Fake*>(object)->kind];return 0;}
HRESULT STDMETHODCALLTYPE extraAcquire(void* object) {int i=static_cast<Fake*>(object)->kind;++acquires[i];return acquireStatus[i];}
HRESULT STDMETHODCALLTYPE extraUnacquire(void* object) {++unacquires[static_cast<Fake*>(object)->kind];return S_OK;}
HRESULT STDMETHODCALLTYPE extraPoll(void* object) {int i=static_cast<Fake*>(object)->kind;++polls[i];return pollStatus[i];}
HRESULT STDMETHODCALLTYPE extraState(void* object,DWORD size,void* output) {
    int i=static_cast<Fake*>(object)->kind;++reads[i];require(size==sizeof(DIJOYSTATE2),"owned state size");
    auto& state=*static_cast<DIJOYSTATE2*>(output);memset(&state,0,sizeof(state));
    state.lZ=pedalValue;state.rgbButtons[0]=0x80;state.rgbButtons[1]=0x80;
    if(i==1&&lostReads>0) {--lostReads;return DIERR_INPUTLOST;}
    return readStatus[i];
}
HRESULT STDMETHODCALLTYPE extraObjects(void* object,void* callback,void* context,DWORD flags) {
    require(flags==DIDFT_ALL,"owned enumeration flags");auto fn=reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKW>(callback);
    DIDEVICEOBJECTINSTANCEW d{};d.dwSize=sizeof(d);
    if(static_cast<Fake*>(object)->kind==1) {d.guidType=GUID_ZAxis;d.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(7);fn(&d,context);}
    else for(int b=0;b<2;++b) {d.guidType=GUID_Button;d.dwType=DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(b);fn(&d,context);}
    return S_OK;
}
HRESULT STDMETHODCALLTYPE extraFormat(void* object,const DIDATAFORMAT* format) {
    require(format->dwDataSize==272&&format->dwFlags==DIDF_ABSAXIS,"owned absolute public format");
    if(static_cast<Fake*>(object)->kind==1) {
        require(format->dwNumObjs==1&&format->rgodf[0].dwOfs==DIJOFS_Z&&DIDFT_GETINSTANCE(format->rgodf[0].dwType)==7,"real pedal instance mapped to Z offset");
    } else {
        require(format->dwNumObjs==2&&format->rgodf[0].dwOfs==DIJOFS_BUTTON(0)&&format->rgodf[1].dwOfs==DIJOFS_BUTTON(1),"physical button numbers retained");
    }
    return formatStatus;
}
HRESULT STDMETHODCALLTYPE extraRange(void*,REFGUID,const DIPROPHEADER* header) {
    auto range=reinterpret_cast<const DIPROPRANGE*>(header);
    require(header->dwHow==DIPH_BYOFFSET&&header->dwObj==DIJOFS_Z&&range->lMin==-1000&&range->lMax==1000,"private pedal axis range normalization");return S_OK;
}
HRESULT STDMETHODCALLTYPE extraCoop(void*,HWND window,DWORD flags) {
    require(window==reinterpret_cast<HWND>(123)&&flags==(DISCL_FOREGROUND|DISCL_NONEXCLUSIVE),"owned foreground nonexclusive cooperative level");return S_OK;
}
void seedRoot() {
    rootTable[2]=reinterpret_cast<void*>(&extraRelease);rootTable[3]=reinterpret_cast<void*>(&extraCreate);
    obs::attach(&root,true,true,GUID_NULL);
    compat::sourceRoot=&root;require(compat::nativeMethods(&root,compat::rootMethods,11),"unhooked root methods captured");
}
void configureTables() {
    for(void** table:{pedalTable,boxTable}) {
        table[2]=reinterpret_cast<void*>(&extraRelease);table[4]=reinterpret_cast<void*>(&extraObjects);
        table[6]=reinterpret_cast<void*>(&extraRange);table[7]=reinterpret_cast<void*>(&extraAcquire);table[8]=reinterpret_cast<void*>(&extraUnacquire);
        table[9]=reinterpret_cast<void*>(&extraState);table[11]=reinterpret_cast<void*>(&extraFormat);table[13]=reinterpret_cast<void*>(&extraCoop);table[25]=reinterpret_cast<void*>(&extraPoll);
    }
    // Prove owned reads bypass already-patched Windows-native vtables.
    obs::attach(&pedals,false,true,GUID_NULL);obs::attach(&box,false,true,GUID_NULL);
}
void sample(ULONGLONG time,bool focused,DIJOYSTATE2* states,bool* valid) {
    memset(states,0,sizeof(DIJOYSTATE2)*compat::maxSources);memset(valid,0,sizeof(bool)*compat::maxSources);
    AcquireSRWLockExclusive(&compat::extraLock);
    compat::sampleSources(focused,reinterpret_cast<HWND>(123),time,states,valid);
    ReleaseSRWLockExclusive(&compat::extraLock);
}
int main() {
    QueryPerformanceFrequency(&obs::frequency);
    wchar_t path[MAX_PATH];GetFullPathNameW(L"build\\compatibility\\multi-test.ini",MAX_PATH,path,nullptr);
    require(CopyFileW(L"tests\\virpil-fixture.ini",path,FALSE)!=0,"fixture copy");
    WritePrivateProfileStringW(L"Compatibility",L"Version",L"2",path);
    WritePrivateProfileStringW(L"Stick.Bindings",L"Yaw",L"None",path);
    WritePrivateProfileStringW(L"Stick.Bindings",L"Gun",L"None",path);
    WritePrivateProfileStringW(L"Throttle.Bindings",L"Pause",L"None",path);
    WritePrivateProfileStringW(L"Device.Pedals",L"InstanceGUID",L"{33333333-3333-4333-8333-333333333333}",path);
    WritePrivateProfileStringW(L"Device.Pedals.Bindings",L"Yaw",L"Z",path);
    WritePrivateProfileStringW(L"Device.Box",L"InstanceGUID",L"{44444444-4444-4444-8444-444444444444}",path);
    WritePrivateProfileStringW(L"Device.Box.Bindings",L"Gun",L"Button1",path);
    WritePrivateProfileStringW(L"Device.Box.Bindings",L"Pause",L"Button2",path);
    require(compat::load(path)&&compat::sourceCount==4,"additional devices configured");compat::active=true;
    require(compat::role(compat::instances[0])==1&&compat::role(compat::instances[1])==2&&compat::role(compat::sources[2].instance)==0&&compat::additional(compat::sources[2].instance),"same two identities; private source has no presented role");
    // External yaw advertises Rz using the otherwise-unused real Z object.
    DWORD advertised=0;DIDEVICEOBJECTINSTANCEW object{};object.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(2);object.guidType=GUID_ZAxis;
    compat::objectIdentity(1,object,&advertised);
    require(object.guidType==GUID_RzAxis&&DIDFT_GETINSTANCE(object.dwType)==2&&compat::externalOutputsReady(1,advertised),"external yaw retains real anchor object ID and virtual Rz capability");
    require(!compat::externalOutputsReady(1,0),"missing anchor carrier rejected");
    configureTables();seedRoot();DIJOYSTATE2 states[compat::maxSources]{},physical{},mapped{};bool valid[compat::maxSources]{};
    physical.lX=123;physical.lY=-321;physical.lZ=-1000;physical.rgbButtons[1]=0x80;
    sample(100,true,states,valid);require(valid[2]&&valid[3]&&creates[1]==1&&creates[2]==1,"two independently owned extra sources ready");
    compat::translateSources(1,physical,states,valid,mapped);
    require(mapped.lRz==750&&mapped.lX==123&&mapped.lY==-321&&mapped.rgbButtons[0]==0x80,"pedals replace stick yaw and box supplies gun; other axes preserved");
    compat::translateSources(2,physical,states,valid,mapped);require(mapped.rgbButtons[24]==0x80,"same box supplies action on second game-facing role");
    int prior=reads[1];sample(101,true,states,valid);require(reads[1]==prior,"two role reads reuse bounded two-millisecond snapshot");
    readStatus[1]=E_PENDING;sample(110,true,states,valid);require(!valid[2]&&valid[3],"partial read failure isolated");
    compat::translateSources(1,physical,states,valid,mapped);require(mapped.lRz==0&&mapped.rgbButtons[0]==0x80,"failure neutralizes yaw instead of holding stale pedals or reverting to stick");
    readStatus[1]=S_OK;lostReads=1;prior=acquires[1];sample(120,true,states,valid);require(valid[2]&&acquires[1]==prior+1,"one bounded input-loss reacquisition");
    lostReads=8;prior=reads[1];sample(130,true,states,valid);require(!valid[2]&&reads[1]==prior+2,"persistent loss returns after two attempts");lostReads=0;
    sample(140,false,states,valid);require(!valid[2]&&!valid[3]&&!compat::owned[2].acquired&&!compat::owned[3].acquired,"focus loss releases owned acquisition and clears states");
    acquireStatus[1]=DIERR_OTHERAPPHASPRIO;prior=reads[1];sample(150,true,states,valid);require(!valid[2]&&valid[3]&&reads[1]==prior,"priority denial cannot poll or leak stale pedal state");acquireStatus[1]=S_OK;
    sample(160,true,states,valid);require(valid[2],"focus return recovers input");
    SetLastError(776);compat::merge(1,physical,mapped);
    require(GetLastError()==776&&mapped.lRz==0&&mapped.rgbButtons[0]==0&&!compat::owned[2].valid,
        "production merge rejects absent game focus, clears cached external state and preserves LastError");
    // Configured additional devices are hidden only by the active proxy's
    // callbacks; their Windows identity and real GUIDs remain unchanged.
    int callbacks=0;auto inventory=[](const DIDEVICEINSTANCEW*,void* value)->BOOL {++*static_cast<int*>(value);return DIENUM_CONTINUE;};
    obs::EnumContext context{reinterpret_cast<void*>(static_cast<LPDIENUMDEVICESCALLBACKW>(inventory)),&callbacks,true,1};
    DIDEVICEINSTANCEW deviceInfo{};deviceInfo.dwSize=sizeof(deviceInfo);deviceInfo.guidInstance=compat::sources[2].instance;
    SetLastError(779);require(obs::enumW(&deviceInfo,&context)==DIENUM_CONTINUE&&callbacks==0&&GetLastError()==779,
        "private source filtered without exposing a third presented controller");
    deviceInfo.guidInstance=GUID_NULL;obs::enumW(&deviceInfo,&context);require(callbacks==1,"unconfigured controller enumeration preserved");
    SetLastError(777);compat::suspendExtras();require(GetLastError()==777,"owned suspension preserves LastError");
    SetLastError(778);compat::stopExtras();require(GetLastError()==778&&!compat::sourceRoot&&!compat::owned[2].object&&!compat::owned[3].object&&releases[1]==1&&releases[2]==1&&releases[0]==1,"owned objects released exactly once outside DllMain");
    // Setup failures do not change anchor translation or retry every frame.
    seedRoot();formatStatus=DIERR_INVALIDPARAM;prior=creates[1];sample(200,true,states,valid);require(!valid[2]&&!valid[3]&&!compat::owned[2].object,"setup failure releases private device");
    sample(300,true,states,valid);require(creates[1]==prior+1,"setup retries throttled to one second");formatStatus=S_OK;
    sample(1201,true,states,valid);require(valid[2]&&valid[3],"source recovers after retry interval");
    readStatus[1]=DIERR_UNPLUGGED;sample(1210,true,states,valid);
    require(!valid[2]&&!compat::owned[2].object&&valid[3],"unplug releases only failed source");
    prior=creates[1];sample(1212,true,states,valid);require(creates[1]==prior,"unplugged source creation retry is bounded");
    readStatus[1]=S_OK;sample(2211,true,states,valid);require(valid[2],"reconnected source can be recreated after backoff");compat::stopExtras();
    compat::OwnedSource maximum;DIDEVICEOBJECTINSTANCEW capability{};
    for(unsigned axis=0;axis<6;++axis) {capability.guidType=*compat::axisGuids[axis];capability.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(axis);compat::sourceObject(&capability,&maximum);}
    for(unsigned button=0;button<128;++button) {capability.guidType=GUID_Button;capability.dwType=DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(button);compat::sourceObject(&capability,&maximum);}
    require(maximum.formatCount==134&&maximum.present[127],"maximum public axis/button descriptor bounds");
    WritePrivateProfileStringW(L"Stick.Bindings",L"Yaw",L"Z",path);require(!compat::load(path),"two active owners of yaw rejected");
    WritePrivateProfileStringW(L"Stick.Bindings",L"Yaw",L"None",path);WritePrivateProfileStringW(L"Device.Pedals",L"InstanceGUID",L"{11111111-1111-4111-8111-111111111111}",path);require(!compat::load(path),"same physical device cannot be anchor and private source");
    WritePrivateProfileStringW(L"Device.Pedals",L"InstanceGUID",L"{33333333-3333-4333-8333-333333333333}",path);
    WritePrivateProfileStringW(L"Device.Missing.Bindings",L"Yaw",L"Z",path);require(!compat::load(path),"undeclared binding device rejected");WritePrivateProfileStringW(L"Device.Missing.Bindings",nullptr,nullptr,path);
    WritePrivateProfileStringW(L"Compatibility",L"Version",L"1",path);require(!compat::load(path),"v1 refuses additional-device sections rather than silently ignoring them");
    WritePrivateProfileStringW(L"Compatibility",L"Version",L"2",path);
    WritePrivateProfileStringW(L"Device.Pedals.Bindings",L"Yaw",L"-Z",path);require(compat::load(path),"same axis inversion syntax on additional source");
    states[2].lZ=250;valid[2]=true;compat::translateSources(1,physical,states,valid,mapped);require(mapped.lRz==-250,"external yaw inversion");
    WritePrivateProfileStringW(L"Device.Pedals.Bindings",L"TypoAction",L"Button1",path);require(!compat::load(path),"unknown action rejected");
    FILE* reverse=nullptr;require(_wfopen_s(&reverse,path,L"wb")==0&&reverse,"reverse section-order fixture");
    fputs("[Compatibility]\nVersion=2\nEnabled=1\n[Device.Pedals.Bindings]\nYaw=Z\n"
        "[Device.Pedals]\nInstanceGUID={33333333-3333-4333-8333-333333333333}\n"
        "[Stick]\nInstanceGUID={11111111-1111-4111-8111-111111111111}\n"
        "[Throttle]\nInstanceGUID={22222222-2222-4222-8222-222222222222}\n"
        "[Stick.Bindings]\nYaw=None\n",reverse);fclose(reverse);
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,path); // flush Win32 profile cache
    require(compat::load(path),"device declarations resolved regardless of section order");
    states[2].lZ=500;valid[2]=true;compat::translateSources(1,physical,states,valid,mapped);
    require(mapped.lRz==500,"late None cannot erase earlier external action assignment");
    DeleteFileW(path);puts("PASS: multi-device config, source aggregation, anchor capabilities, private COM bypass, failures, focus, bounded reacquisition, retries and cleanup.");return 0;
}

