// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
#include "../src/runtime.cpp"
#include <initializer_list>
void require(bool ok,const char* what) {if(!ok) {printf("FAIL: %s\n",what);exit(1);}}
struct Fake {void** table;}; void* table[32]{}; Fake device{table};
LONG stateCalls,objectCalls,formatCalls; bool failState; int callbackCount; void* contextExpected;
HRESULT STDMETHODCALLTYPE fixtureState(void*,DWORD size,void* p) {
    ++stateCalls;require(GetLastError()==101,"state incoming LastError");SetLastError(102);
    if(failState) return DIERR_NOTACQUIRED;
    require(size==sizeof(DIJOYSTATE2),"source format size");auto& j=*static_cast<DIJOYSTATE2*>(p);
    memset(&j,0,sizeof(j));j.lX=250;j.lY=-500;j.lZ=750;j.rgbButtons[1]=0x80;return S_OK;
}
HRESULT STDMETHODCALLTYPE fixtureFormat(void*,const DIDATAFORMAT* p) {
    ++formatCalls;require(p->dwDataSize==272,"original format forwarded");SetLastError(103);return S_OK;
}
HRESULT STDMETHODCALLTYPE fixtureObjects(void*,void* callback,void* context,DWORD flags) {
    ++objectCalls;require(flags==DIDFT_ALL,"object flags forwarded");
    for(unsigned a=0;a<5;++a) {DIDEVICEOBJECTINSTANCEW d{};d.dwSize=sizeof(d);d.guidType=*compat::axisGuids[a];
        d.dwType=DIDFT_ABSAXIS|DIDFT_MAKEINSTANCE(a);d.dwOfs=a*4;
        if(!reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKW>(callback)(&d,context)) break;
    }
    for(unsigned b=0;b<31;++b) {DIDEVICEOBJECTINSTANCEW d{};d.dwSize=sizeof(d);d.guidType=GUID_Button;
        d.dwType=DIDFT_PSHBUTTON|DIDFT_MAKEINSTANCE(b);d.dwOfs=DIJOFS_BUTTON(b);
        if(!reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKW>(callback)(&d,context)) break;
    }
    SetLastError(104);return S_OK;
}
BOOL CALLBACK objects(const DIDEVICEOBJECTINSTANCEW* d,void* c) {
    require(c==contextExpected,"object callback context");++callbackCount;
    if(d->guidType==GUID_RzAxis) require(DIDFT_GETINSTANCE(d->dwType)==2,"remapped axis keeps real range object ID");
    SetLastError(105);return DIENUM_CONTINUE;
}
HRESULT STDMETHODCALLTYPE fixtureInfo(void*,void* p) {
    auto& d=*static_cast<DIDEVICEINSTANCEW*>(p);d.guidInstance=compat::instances[0];d.guidProduct.Data1=0x40CC3344;
    SetLastError(106);return S_OK;
}
// ABI-correct isolated call stub makes _ReturnAddress() a controlled address.
// No production switch can bypass the supported executable/caller guards.
void* stub(void* fn,DWORD rva) {
    unsigned char bytes[]={0x48,0xB8,0,0,0,0,0,0,0,0,0x48,0x83,0xEC,0x28,0xFF,0xD0,0x48,0x83,0xC4,0x28,0xC3};
    memcpy(bytes+2,&fn,sizeof(fn));void* p=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    require(p!=nullptr,"fixture call stub");memcpy(p,bytes,sizeof(bytes));DWORD old;
    require(VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old)!=0,"fixture executable permission");FlushInstructionCache(GetCurrentProcess(),p,sizeof(bytes));
    compat::image=reinterpret_cast<unsigned char*>(reinterpret_cast<ULONG_PTR>(p)+16-rva);return p;
}
int main() {
    wchar_t ini[MAX_PATH];require(GetFullPathNameW(L"tests\\virpil-fixture.ini",MAX_PATH,ini,nullptr)>0,"fixture config path");
    require(compat::load(ini),"supplied INI parses");
    wchar_t bad[MAX_PATH];GetFullPathNameW(L"build\\compatibility\\invalid-test.ini",MAX_PATH,bad,nullptr);
    require(CopyFileW(ini,bad,FALSE)!=0,"invalid-config fixture copy");
    WritePrivateProfileStringW(L"Stick.Bindings",L"Gun",L"Button0",bad);
    require(!compat::load(bad)&&!compat::enabled(),"bad button disables entire config");
    CopyFileW(ini,bad,FALSE);WritePrivateProfileStringW(L"Stick.Bindings",L"Pitch",L"X",bad);
    require(!compat::load(bad),"duplicate source axis rejected");
    CopyFileW(ini,bad,FALSE);WritePrivateProfileStringW(L"Stick.Binding",L"Gun",L"Button2",bad);
    require(!compat::load(bad),"misspelled binding section rejected");
    CopyFileW(ini,bad,FALSE);WritePrivateProfileStringW(L"Compatibility",L"Enabled",L"0",bad);
    require(!compat::load(bad)&&!compat::enabled(),"explicit disable fails open");
    DeleteFileW(bad);require(compat::load(ini),"restore fixture config");
    compat::active=true; QueryPerformanceFrequency(&obs::frequency);
    require(compat::role(compat::instances[0])==1&&compat::role(compat::instances[1])==2&&compat::role(GUID_NULL)==0,"pinned device roles");
    compat::Binding b;require(!compat::parse(L"Button0",false,b)&&!compat::parse(L"Button129",false,b)&&
        !compat::parse(L"Button2junk",false,b)&&compat::parse(L"Button128",false,b)&&b.index==127,"one-based boundary validation");
    DIJOYSTATE2 in{},out{};in.rgbButtons[1]=0x80;in.rgbButtons[11]=0x80;in.rgbButtons[13]=0x80;in.rgbButtons[14]=0x80;
    in.lX=250;in.lY=-500;in.lZ=750;in.lRx=-250;in.lRy=250;
    compat::translate(1,in,out);
    require(out.rgbButtons[0]==0x80&&out.rgbButtons[1]==0x80&&out.rgbButtons[11]==0,"physical-to-profile numbering and unused buttons");
    require(out.lX==250&&out.lY==-500&&out.lRz==750&&out.lRx==-250&&out.lRy==250&&out.lZ==0,"axis remap with stock pitch inversion compensation");
    require(out.rgdwPOV[0]==4500&&out.rgdwPOV[1]==0xFFFFFFFF,"diagonal and centered unused hats");
    in.rgbButtons[15]=0x80;in.rgbButtons[16]=0x80;compat::translate(1,in,out);require(out.rgdwPOV[0]==0xFFFFFFFF,"opposite hat directions cancel");
    for(LONG x:{-1000L,0L,1000L}) {in.lRx=x;compat::translate(2,in,out);require(out.lX==-x&&out.lY==0,"single throttle channel and neutral second channel");}
    // All known-good VIRPIL assignments, including controls not exercised in
    // the original free-flight capture. No physical device or game is needed.
    struct DigitalCase {int role,source,output;};
    const DigitalCase digitalCases[]={{1,2,1},{1,12,2},{1,6,3},{1,7,14},{1,5,5},
        {2,16,5},{2,8,1},{2,21,3},{2,32,25}};
    for(const auto& c:digitalCases) {
        DIJOYSTATE2 physical{},mapped{};physical.rgbButtons[c.source-1]=0x80;
        compat::translate(c.role,physical,mapped);
        for(int button=0;button<128;++button)
            require(mapped.rgbButtons[button]==(button==c.output-1?0x80:0),"every VIRPIL digital assignment and unused button");
    }
    for(int direction=0;direction<4;++direction) {
        DIJOYSTATE2 physical{},mapped{};physical.rgbButtons[13+direction]=0x80;
        compat::translate(1,physical,mapped);require(mapped.rgdwPOV[0]==static_cast<DWORD>(direction*9000),"all four VIRPIL hat directions");
    }
    const int sourceAxes[]={0,1,2,3,4};const int outputAxes[]={0,1,5,3,4};
    for(int a=0;a<5;++a) for(LONG value:{-1000L,-250L,0L,250L,1000L}) {
        DIJOYSTATE2 physical{},mapped{};
        reinterpret_cast<LONG*>(&physical)[sourceAxes[a]]=value;compat::translate(1,physical,mapped);
        for(int channel=0;channel<6;++channel)
            require(reinterpret_cast<LONG*>(&mapped)[channel]==(channel==outputAxes[a]?value:0),"every VIRPIL axis and neutral unused channel");
    }
    DIDEVICEINSTANCEW info{};info.dwSize=sizeof(info);info.guidInstance=compat::instances[0];info.guidProduct.Data1=0x40CC3344;
    compat::identity(info);require(info.guidInstance==compat::instances[0]&&info.guidProduct.Data1==0x22210738,"virtual product preserves instance GUID");
    DIDEVICEINSTANCEA ansi{};ansi.guidInstance=compat::instances[1];compat::identity(ansi);require(ansi.guidProduct.Data1==0xA2210738,"ANSI identity");
    info.guidInstance=GUID_NULL;info.guidProduct.Data1=99;compat::identity(info);require(info.guidProduct.Data1==99,"unconfigured identity untouched");
    require(compat::supported(1,0x1F,31)&&!compat::supported(1,0x1B,31)&&!compat::supported(1,0x1F,10),"missing source inputs rejected");
    unsigned char present[128];memset(present,1,sizeof(present));present[1]=0;
    require(!compat::supported(1,0x1F,31,present),"sparse missing physical button rejected");
    table[4]=reinterpret_cast<void*>(&fixtureObjects);table[9]=reinterpret_cast<void*>(&fixtureState);
    table[11]=reinterpret_cast<void*>(&fixtureFormat);table[12]=reinterpret_cast<void*>(&fixtureInfo);
    obs::attach(&device,false,true,compat::instances[0]);auto e=obs::find(&device);require(e!=nullptr,"COM tracking");
    int token=7;contextExpected=&token;void* p=stub(table[4],0x4E696A1);
    require(reinterpret_cast<obs::Objects>(p)(&device,reinterpret_cast<void*>(&objects),&token,DIDFT_ALL)==S_OK&&GetLastError()==104&&objectCalls==1,"enum original exactly once and result/LastError preserved");
    require(callbackCount==37&&e->compatibilityObjects,"physical objects plus synthesized POV capability");VirtualFree(p,0,MEM_RELEASE);
    DIDATAFORMAT format{};format.dwSize=sizeof(format);format.dwObjSize=sizeof(DIOBJECTDATAFORMAT);format.dwDataSize=272;format.dwNumObjs=164;format.dwFlags=DIDF_ABSAXIS;
    p=stub(table[11],0x4E696BC);require(reinterpret_cast<obs::Format>(p)(&device,&format)==S_OK&&formatCalls==1&&e->compatibilityFormat&&GetLastError()==103,"verified game format gating");VirtualFree(p,0,MEM_RELEASE);
    p=stub(table[9],0x4E699CE);SetLastError(101);
    require(reinterpret_cast<obs::State>(p)(&device,272,&out)==S_OK&&stateCalls==1&&GetLastError()==102&&out.lRz==750&&out.rgbButtons[0]==0x80,"translated native state call exactly once");
    failState=true;memset(&out,0xCD,sizeof(out));DIJOYSTATE2 sentinel=out;SetLastError(101);
    require(reinterpret_cast<obs::State>(p)(&device,272,&out)==DIERR_NOTACQUIRED&&stateCalls==2&&GetLastError()==102&&!memcmp(&out,&sentinel,sizeof(out)),"failed reads and caller buffer untouched");
    VirtualFree(p,0,MEM_RELEASE);failState=false;SetLastError(101);
    require(reinterpret_cast<obs::State>(table[9])(&device,272,&out)==S_OK&&out.lZ==750&&out.lRz==0&&out.rgbButtons[1]==0x80,"other callers retain native state");
    info={};info.dwSize=sizeof(info);require(reinterpret_cast<obs::Info>(table[12])(&device,&info)==S_OK&&info.guidProduct.Data1==0x22210738&&GetLastError()==106,"GetDeviceInfo consistent identity");
    compat::active=false;SetLastError(101);reinterpret_cast<obs::State>(table[9])(&device,272,&out);require(out.lZ==750&&out.lRz==0,"disabled forwarding");
    require(!compat::version(GetModuleHandleW(nullptr)),"unsupported executable rejected");
    // Load the actual DLL in this test process. Its executable guard must leave
    // native Windows enumeration intact; no acquire, polling or state reads.
    wchar_t dll[MAX_PATH],log[MAX_PATH];GetFullPathNameW(L"build\\compatibility\\dinput8.dll",MAX_PATH,dll,nullptr);
    GetFullPathNameW(L"build\\compatibility\\load-test.log",MAX_PATH,log,nullptr);SetEnvironmentVariableW(L"AC8_DI_LOG",log);
    wchar_t runtimeIni[MAX_PATH];GetFullPathNameW(L"build\\compatibility\\AC8InputMapper.ini",MAX_PATH,runtimeIni,nullptr);
    require(CopyFileW(ini,runtimeIni,FALSE)!=0,"new runtime filename with enabled known-good configuration");
    HMODULE loaded=LoadLibraryExW(dll,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(loaded!=nullptr,"built DLL loads without additional runtime files");
    const char* names[]={"DirectInput8Create","DllCanUnloadNow","DllGetClassObject","DllRegisterServer","DllUnregisterServer","GetdfDIJoystick"};
    for(unsigned i=0;i<6;++i) require(GetProcAddress(loaded,names[i])&&GetProcAddress(loaded,names[i])==GetProcAddress(loaded,MAKEINTRESOURCEA(i+1)),"six exact exported names/ordinals");
    auto make=reinterpret_cast<decltype(&DirectInput8Create)>(GetProcAddress(loaded,"DirectInput8Create"));IDirectInput8W* di=nullptr;
    require(SUCCEEDED(make(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&di),nullptr))&&di,"actual DLL forwards system DirectInput creation");
    GUID first{};auto inventory=[](const DIDEVICEINSTANCEW* d,void* c)->BOOL { *static_cast<GUID*>(c)=d->guidInstance;return DIENUM_STOP; };
    require(SUCCEEDED(di->EnumDevices(DI8DEVCLASS_GAMECTRL,inventory,&first,DIEDFL_ATTACHEDONLY)),"actual DLL forwards native enumeration");
    if(first!=GUID_NULL) {IDirectInputDevice8W* input=nullptr;require(SUCCEEDED(di->CreateDevice(first,&input,nullptr)),"real device creation");
        DIDEVICEINSTANCEW d{};d.dwSize=sizeof(d);require(SUCCEEDED(input->GetDeviceInfo(&d))&&d.guidInstance==first,"real device identity forwarded");input->Release();}
    di->Release(); // DLL remains loaded until exit because native vtables are patched.
    require(CopyFileW(L"AC8InputMapper.example.ini",runtimeIni,FALSE)!=0,"restore blank runtime template");
    puts("PASS: compatibility config, identities, button/axis/POV translation, caller gates, COM failures, actual DLL exports/load/system forwarding.");
    return 0;
}
