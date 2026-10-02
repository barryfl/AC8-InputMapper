// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
// Public DirectInput8 ABI only. Never calls an input method on its own.
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <new>
#include <intrin.h>
namespace profile {
void begin(void*,REFGUID,void*);
void released(void*,void*,void*,ULONG);
void acquired(void*,void*,HRESULT);
}
#ifdef AC8_COMPAT
#include "compat.h"
#endif

namespace obs {
HMODULE self, real;
INIT_ONCE once = INIT_ONCE_STATIC_INIT;
SRWLOCK outputLock = SRWLOCK_INIT, entriesLock = SRWLOCK_INIT;
HANDLE file = INVALID_HANDLE_VALUE;
LONG64 sequence, callSequence;
LARGE_INTEGER frequency;
thread_local bool writing;
// Keep the native vtable pointer, its private prefix, and private trailing slots
// intact. Patch only public method slots in-place, once per native table. All
// objects using such a table are observed; unseen objects are registered lazily.
// Tracking records live until process exit; no extra COM references are taken.
struct Entry {
    Entry* next; void* object; void* original[32];
    bool root, wide, alive; GUID guid; LONG64 polls, reads;
    int compatibilityRole; bool compatibilityFormat, compatibilityObjects;
    HRESULT lastPoll, lastState; DWORD stateSize, formatSize; bool standardShape;
    unsigned char state[4096]; LONGLONG lastLog;
};
struct Hooks { Hooks* next; void** native; void* original[32]; bool root, wide; Entry fallback; };
Hooks* tables;
Entry* entries;
bool copy(void* to, const void* from, size_t n) {
    if (!n) return true;
    if (!from) return false;
    // Do not touch guard/no-access/uncommitted pages, including across regions.
    auto p = static_cast<const unsigned char*>(from); size_t left = n;
    while (left) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(p, &m, sizeof(m)) || m.State != MEM_COMMIT ||
            (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(m.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                          PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
        size_t available = static_cast<const unsigned char*>(m.BaseAddress) + m.RegionSize - p;
        size_t step = left < available ? left : available;
        if (!step) return false;
        p += step; left -= step;
    }
    __try { memcpy(to, from, n); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void log(const char* fmt, ...) {
    DWORD saved = GetLastError();
    if (writing || file == INVALID_HANDLE_VALUE) { SetLastError(saved); return; }
    writing = true;
    char b[12000]; LARGE_INTEGER q; QueryPerformanceCounter(&q);
    AcquireSRWLockExclusive(&outputLock);
    int n = _snprintf_s(b, sizeof(b), _TRUNCATE, "seq=%lld qpc=%lld tid=%lu ",
        InterlockedIncrement64(&sequence), q.QuadPart, GetCurrentThreadId());
    if (n > 0) {
        va_list a; va_start(a, fmt); int k = _vsnprintf_s(b+n, sizeof(b)-n, _TRUNCATE, fmt, a); va_end(a);
        size_t len = k >= 0 ? static_cast<size_t>(n+k) : strlen(b);
        if (len + 1 < sizeof(b)) b[len++] = '\n';
        DWORD written; WriteFile(file, b, static_cast<DWORD>(len), &written, nullptr);
    }
    ReleaseSRWLockExclusive(&outputLock);
    writing = false; SetLastError(saved);
}
void guidText(REFGUID g, char* out) {
    sprintf_s(out, 40, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        g.Data1,g.Data2,g.Data3,g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],
        g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);
}
Entry* find(void* object) {
    AcquireSRWLockExclusive(&entriesLock); Entry* e = entries;
    while (e && (e->object != object || !e->alive)) e = e->next;
    if(!e) {
        void** native=*static_cast<void***>(object);
        for(Hooks* h=tables;h;h=h->next) if(h->native==native) {
            e=static_cast<Entry*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Entry)));
            if(e) { e->object=object; e->root=h->root; e->wide=h->wide; e->alive=true;
                memcpy(e->original,h->original,sizeof(e->original)); e->next=entries; entries=e; }
            else e=&h->fallback; // Preserve forwarding even if tracking allocation fails.
            break;
        }
    }
    ReleaseSRWLockExclusive(&entriesLock); return e;
}
using Simple = HRESULT (STDMETHODCALLTYPE*)(void*);
using Create = HRESULT (STDMETHODCALLTYPE*)(void*,REFGUID,void**,IUnknown*);
using Enum = HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,void*,void*,DWORD);
using State = HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,void*);
using Data = HRESULT (STDMETHODCALLTYPE*)(void*,DWORD,DIDEVICEOBJECTDATA*,DWORD*,DWORD);
using Format = HRESULT (STDMETHODCALLTYPE*)(void*,const DIDATAFORMAT*);
using Coop = HRESULT (STDMETHODCALLTYPE*)(void*,HWND,DWORD);
using Property = HRESULT (STDMETHODCALLTYPE*)(void*,REFGUID,const DIPROPHEADER*);
using Release = ULONG (STDMETHODCALLTYPE*)(void*);
void attach(void*,bool,bool,REFGUID);
ULONG STDMETHODCALLTYPE release(void* object) {
    DWORD before=GetLastError(); Entry* e=find(object); auto fn=reinterpret_cast<Release>(e->original[2]);
    void* caller=_ReturnAddress(); void* returnSlot=_AddressOfReturnAddress();
    SetLastError(before); ULONG refs=fn(object); DWORD after=GetLastError();
    profile::released(object,caller,returnSlot,refs);
    if(!refs&&compat::role(e->guid)) compat::stopExtras();
    if(!refs) { AcquireSRWLockExclusive(&entriesLock); e->alive=false; ReleaseSRWLockExclusive(&entriesLock); }
    SetLastError(after); return refs;
}
HRESULT STDMETHODCALLTYPE create(void* o, REFGUID g, void** out, IUnknown* outer) {
    DWORD before = GetLastError(); Entry* e = find(o); auto fn = reinterpret_cast<Create>(e->original[3]);
    LONG64 call=InterlockedIncrement64(&callSequence);
    char id[40]; guidText(g,id); log("CreateDevice enter call=%lld caller=%p object=%p instance=%s out=%p outer=%p",call,_ReturnAddress(),o,id,out,outer);
    SetLastError(before); HRESULT hr = fn(o,g,out,outer); DWORD after = GetLastError();
    void* d = nullptr; if (SUCCEEDED(hr) && !outer && copy(&d,out,sizeof(d)) && d) attach(d,false,e->wide,g);
    log("CreateDevice exit call=%lld object=%p instance=%s hr=%08lX device=%p",call,o,id,hr,d);
    SetLastError(after); return hr;
}
struct EnumContext { void* callback; void* context; bool wide; LONG64 call; };
void wideText(const WCHAR* p, char* b, int capacity) {
    WCHAR text[MAX_PATH]; memcpy(text,p,sizeof(text)); text[MAX_PATH-1]=0;
    if (!WideCharToMultiByte(CP_UTF8,0,text,-1,b,capacity,nullptr,nullptr)) strcpy_s(b,capacity,"<conversion failed>");
    for (char* c=b; *c; ++c) if (*c=='\n'||*c=='\r'||*c=='"') *c=' ';
}
BOOL CALLBACK enumW(const DIDEVICEINSTANCEW* p, void* context) {
    auto c=static_cast<EnumContext*>(context); DWORD before=GetLastError(); DIDEVICEINSTANCEW d{};
    if (copy(&d,p,sizeof(d))) {
        char g[40], product[40], name[1024], instance[1024]; guidText(d.guidInstance,g); guidText(d.guidProduct,product);
        wideText(d.tszProductName,name,sizeof(name)); wideText(d.tszInstanceName,instance,sizeof(instance));
        log("EnumDevice parent=%lld instance=%s product=%s type=%08lX name=\"%s\" instance_name=\"%s\"",c->call,g,product,d.dwDevType,name,instance);
    } else log("EnumDevice parent=%lld unreadable=%p",c->call,p);
#ifdef AC8_COMPAT
    if(copy(&d,p,sizeof(d))&&compat::additional(d.guidInstance)) {SetLastError(before);return DIENUM_CONTINUE;}
    if(copy(&d,p,sizeof(d))&&compat::role(d.guidInstance)) {compat::identity(d);SetLastError(before);return reinterpret_cast<LPDIENUMDEVICESCALLBACKW>(c->callback)(&d,c->context);}
#endif
    SetLastError(before); return reinterpret_cast<LPDIENUMDEVICESCALLBACKW>(c->callback)(p,c->context);
}
BOOL CALLBACK enumA(const DIDEVICEINSTANCEA* p, void* context) {
    auto c=static_cast<EnumContext*>(context); DWORD before=GetLastError(); DIDEVICEINSTANCEA d{};
    if (copy(&d,p,sizeof(d))) {
        char g[40], product[40]; guidText(d.guidInstance,g); guidText(d.guidProduct,product); d.tszProductName[MAX_PATH-1]=0;
        for(char* s=d.tszProductName;*s;++s) if(*s=='\n'||*s=='\r'||*s=='"') *s=' ';
        log("EnumDevice parent=%lld instance=%s product=%s type=%08lX name=\"%s\" encoding=ANSI",c->call,g,product,d.dwDevType,d.tszProductName);
    } else log("EnumDevice parent=%lld unreadable=%p",c->call,p);
#ifdef AC8_COMPAT
    if(copy(&d,p,sizeof(d))&&compat::additional(d.guidInstance)) {SetLastError(before);return DIENUM_CONTINUE;}
    if(copy(&d,p,sizeof(d))&&compat::role(d.guidInstance)) {compat::identity(d);SetLastError(before);return reinterpret_cast<LPDIENUMDEVICESCALLBACKA>(c->callback)(&d,c->context);}
#endif
    SetLastError(before); return reinterpret_cast<LPDIENUMDEVICESCALLBACKA>(c->callback)(p,c->context);
}
HRESULT STDMETHODCALLTYPE enumerate(void* o,DWORD type,void* callback,void* context,DWORD flags) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Enum>(e->original[4]);
    LONG64 id=InterlockedIncrement64(&callSequence); EnumContext c{callback,context,e->wide,id};
    log("EnumDevices enter call=%lld caller=%p object=%p class=%lu flags=%08lX callback=%p context=%p",id,_ReturnAddress(),o,type,flags,callback,context);
    void* replacement=e->wide ? reinterpret_cast<void*>(&enumW) : reinterpret_cast<void*>(&enumA);
    SetLastError(before); HRESULT hr=fn(o,type,callback ? replacement : nullptr,callback ? &c : context,flags); DWORD after=GetLastError();
    log("EnumDevices exit call=%lld hr=%08lX",id,hr); SetLastError(after); return hr;
}
template<unsigned Slot> HRESULT STDMETHODCALLTYPE simple(void* o) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Simple>(e->original[Slot]);
    LONG64 call=InterlockedIncrement64(&callSequence);
    if constexpr(Slot!=25) log("%s enter call=%lld caller=%p device=%p",Slot==7?"Acquire":"Unacquire",call,_ReturnAddress(),o);
    SetLastError(before); HRESULT hr=fn(o); DWORD after=GetLastError();
    if constexpr(Slot==7) profile::acquired(o,_ReturnAddress(),hr);
    if constexpr(Slot==8) if(SUCCEEDED(hr)&&compat::role(e->guid)) compat::suspendExtras();
    if constexpr(Slot==25) {
        AcquireSRWLockExclusive(&entriesLock); LONG64 n=++e->polls;
        bool emit=n==1||hr!=e->lastPoll||(n%256)==0; e->lastPoll=hr; ReleaseSRWLockExclusive(&entriesLock);
        if(emit) log("Poll call=%lld caller=%p device=%p count=%lld hr=%08lX",call,_ReturnAddress(),o,n,hr);
    } else log("%s exit call=%lld device=%p hr=%08lX",Slot==7?"Acquire":"Unacquire",call,o,hr);
    SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE format(void* o,const DIDATAFORMAT* p) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Format>(e->original[11]); DIDATAFORMAT d{};
    LONG64 call=InterlockedIncrement64(&callSequence);
    bool valid=copy(&d,p,sizeof(d));
    log("SetDataFormat enter call=%lld caller=%p device=%p pointer=%p readable=%d size=%lu object_size=%lu flags=%08lX state_size=%lu objects=%lu object_table=%p",
        call,_ReturnAddress(),o,p,valid,d.dwSize,d.dwObjSize,d.dwFlags,d.dwDataSize,d.dwNumObjs,d.rgodf);
    if(valid&&d.rgodf&&d.dwObjSize==sizeof(DIOBJECTDATAFORMAT)) {
        DWORD cap=d.dwNumObjs<256?d.dwNumObjs:256;
        for(DWORD i=0;i<cap;++i) { DIOBJECTDATAFORMAT object{};
            if(!copy(&object,d.rgodf+i,sizeof(object))) { log("FormatObject unreadable device=%p index=%lu",o,i); break; }
            GUID g{}; char id[40]="<unreadable>"; if(!object.pguid) strcpy_s(id,"<null wildcard>"); else if(copy(&g,object.pguid,sizeof(g))) guidText(g,id);
            log("FormatObject device=%p index=%lu offset=%lu type=%08lX flags=%08lX guid=%s",o,i,object.dwOfs,object.dwType,object.dwFlags,id);
        }
        if(d.dwNumObjs>cap) log("FormatObject truncated device=%p total=%lu captured=%lu",o,d.dwNumObjs,cap);
    }
    SetLastError(before); HRESULT hr=fn(o,p); DWORD after=GetLastError();
    if(SUCCEEDED(hr)&&valid) { AcquireSRWLockExclusive(&entriesLock); e->formatSize=d.dwDataSize;
        e->standardShape=d.dwSize==sizeof(DIDATAFORMAT)&&d.dwObjSize==sizeof(DIOBJECTDATAFORMAT)&&d.dwDataSize==272&&d.dwNumObjs==164&&d.dwFlags==DIDF_ABSAXIS;
        ReleaseSRWLockExclusive(&entriesLock); }
#ifdef AC8_COMPAT
    e->compatibilityRole=compat::role(e->guid);
    e->compatibilityFormat=SUCCEEDED(hr)&&e->standardShape&&e->compatibilityObjects&&compat::caller(_ReturnAddress(),0x4E696BC);
    if(e->compatibilityRole&&e->compatibilityFormat) log("Compatibility device=%p role=%d game_format=1",o,e->compatibilityRole);
#endif
    log("SetDataFormat exit call=%lld device=%p hr=%08lX",call,o,hr); SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE cooperative(void* o,HWND window,DWORD flags) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Coop>(e->original[13]);
    LONG64 call=InterlockedIncrement64(&callSequence);
    log("SetCooperativeLevel enter call=%lld caller=%p device=%p hwnd=%p flags=%08lX",call,_ReturnAddress(),o,window,flags);
    SetLastError(before); HRESULT hr=fn(o,window,flags); DWORD after=GetLastError();
    if(SUCCEEDED(hr)&&compat::role(e->guid)) compat::gameWindow(window);
    log("SetCooperativeLevel exit call=%lld device=%p hr=%08lX",call,o,hr); SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE property(void* o,REFGUID g,const DIPROPHEADER* p) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Property>(e->original[6]);
    LONG64 call=InterlockedIncrement64(&callSequence);
    DIPROPHEADER header{}; bool valid=copy(&header,p,sizeof(header)); char id[40]="<unreadable>"; GUID guid{};
    ULONG_PTR address=reinterpret_cast<ULONG_PTR>(&g);
    if(address<=65535) sprintf_s(id,"DIPROP_ID_%llu",static_cast<unsigned long long>(address));
    else if(copy(&guid,&g,sizeof(guid))) guidText(guid,id);
    unsigned char bytes[256]; char hex[513]="<not decoded>";
    if(valid&&header.dwSize>=sizeof(header)&&header.dwSize<=sizeof(bytes)&&copy(bytes,p,header.dwSize))
        for(DWORD i=0;i<header.dwSize;++i) sprintf_s(hex+i*2,3,"%02x",bytes[i]);
    log("SetProperty enter call=%lld caller=%p device=%p property=%s pointer=%p header_readable=%d size=%lu header_size=%lu object=%lu how=%lu raw=%s",
        call,_ReturnAddress(),o,id,p,valid,header.dwSize,header.dwHeaderSize,header.dwObj,header.dwHow,hex);
    SetLastError(before); HRESULT hr=fn(o,g,p); DWORD after=GetLastError();
    if(hr==S_OK) profile::begin(o,e->guid,_ReturnAddress());
    log("SetProperty exit call=%lld device=%p hr=%08lX",call,o,hr); SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE state(void* o,DWORD size,void* p) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<State>(e->original[9]);
    LONG64 call=InterlockedIncrement64(&callSequence);
    SetLastError(before); HRESULT hr=fn(o,size,p); DWORD after=GetLastError();
#ifdef AC8_COMPAT
    if(SUCCEEDED(hr)&&size==sizeof(DIJOYSTATE2)&&e->compatibilityRole&&e->compatibilityFormat&&
        (compat::caller(_ReturnAddress(),0x4E699CE)||compat::caller(_ReturnAddress(),0x4E69A05))) {
        DIJOYSTATE2 source{},translated{};
        if(copy(&source,p,sizeof(source))) {
            compat::sourceLog(e->compatibilityRole,source);
            compat::merge(e->compatibilityRole,source,translated);
            memcpy(p,&translated,sizeof(translated));
            if(e->reads==0) log("Compatibility first_state device=%p role=%d physical_button_numbering=one_based",o,e->compatibilityRole);
        }
    }
#endif
    unsigned char bytes[4096]; bool valid=SUCCEEDED(hr)&&size<=sizeof(bytes)&&copy(bytes,p,size);
    LARGE_INTEGER now; QueryPerformanceCounter(&now); AcquireSRWLockExclusive(&entriesLock);
    LONG64 n=++e->reads; bool emit=n==1||hr!=e->lastState||
        (valid&&(e->stateSize!=size||memcmp(e->state,bytes,size)))||now.QuadPart-e->lastLog>frequency.QuadPart*2;
    if(valid) { memcpy(e->state,bytes,size); e->stateSize=size; }
    if(emit) e->lastLog=now.QuadPart; DWORD fs=e->formatSize; bool standard=e->standardShape; e->lastState=hr; ReleaseSRWLockExclusive(&entriesLock);
    if(emit) {
        char hex[8193]=""; if(valid) for(DWORD i=0;i<size;++i) sprintf_s(hex+i*2,3,"%02x",bytes[i]);
        log("GetDeviceState call=%lld caller=%p device=%p count=%lld size=%lu pointer=%p hr=%08lX valid=%d format_size=%lu raw=%s",call,_ReturnAddress(),o,n,size,p,hr,valid,fs,valid?hex:"<not decoded>");
        // Conventional view for the demonstrated descriptor shape. Object
        // offsets/GUIDs and raw bytes remain the authoritative layout evidence.
        if(valid&&size==sizeof(DIJOYSTATE2)&&standard) {
            DIJOYSTATE2 j; memcpy(&j,bytes,sizeof(j)); char buttons[1024]=""; size_t used=0;
            for(unsigned i=0;i<128;++i) if(j.rgbButtons[i]&0x80) used+=sprintf_s(buttons+used,sizeof(buttons)-used,"%u,",i);
            log("State272 conventional_view device=%p axes=%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld pov=%lu,%lu,%lu,%lu pressed_button_indices_zero_based=%s",
                o,j.lX,j.lY,j.lZ,j.lRx,j.lRy,j.lRz,j.rglSlider[0],j.rglSlider[1],j.rgdwPOV[0],j.rgdwPOV[1],j.rgdwPOV[2],j.rgdwPOV[3],buttons);
        }
    }
    SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE data(void* o,DWORD stride,DIDEVICEOBJECTDATA* p,DWORD* count,DWORD flags) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Data>(e->original[10]); DWORD requested=0;
    LONG64 call=InterlockedIncrement64(&callSequence);
    bool in=copy(&requested,count,sizeof(requested));
    log("GetDeviceData enter call=%lld caller=%p device=%p stride=%lu pointer=%p count_pointer=%p requested=%lu readable=%d flags=%08lX",call,_ReturnAddress(),o,stride,p,count,requested,in,flags);
    SetLastError(before); HRESULT hr=fn(o,stride,p,count,flags); DWORD after=GetLastError(); DWORD returned=0;
    bool out=SUCCEEDED(hr)&&copy(&returned,count,sizeof(returned));
    log("GetDeviceData exit call=%lld device=%p hr=%08lX count_valid=%d returned=%lu",call,o,hr,out,returned);
    if(out&&p&&stride>=16&&stride<=256) {
        DWORD cap=returned<256?returned:256; if(in&&requested!=INFINITE&&cap>requested) cap=requested;
        for(DWORD i=0;i<cap;++i) { DWORD values[4];
            if(!copy(values,reinterpret_cast<unsigned char*>(p)+static_cast<size_t>(i)*stride,sizeof(values))) { log("Buffered unreadable device=%p index=%lu",o,i); break; }
            log("Buffered device=%p index=%lu offset=%lu data=%08lX timestamp=%lu sequence=%lu",o,i,values[0],values[1],values[2],values[3]);
        }
        if(returned>cap) log("Buffered truncated device=%p captured=%lu returned=%lu",o,cap,returned);
    }
    SetLastError(after); return hr;
}
#ifdef AC8_COMPAT
#include "hooks.h"
#endif
void attach(void* object,bool root,bool wide,REFGUID guid) {
    AcquireSRWLockExclusive(&entriesLock);
    void** native=nullptr; Hooks* h=nullptr;
    if(copy(&native,object,sizeof(native))) for(h=tables;h&&h->native!=native;h=h->next) {}
    bool installed=true;
    if(!h&&native) {
        h=static_cast<Hooks*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Hooks)));
        DWORD old=0;
        if(!h||!copy(h->original,native,(root?11:32)*sizeof(void*))||
            !VirtualProtect(native,(root?11:32)*sizeof(void*),PAGE_READWRITE,&old)) {
            if(h) HeapFree(GetProcessHeap(),0,h); h=nullptr; installed=false;
        } else {
            h->native=native; h->root=root; h->wide=wide;
            h->fallback.root=root; h->fallback.wide=wide; memcpy(h->fallback.original,h->original,sizeof(h->original));
            h->next=tables; tables=h;
            // Publish routing before publishing hooks. Concurrent calls wait
            // on entriesLock until this transaction completes.
            auto patch=[native](unsigned slot,void* fn) { InterlockedExchangePointer(reinterpret_cast<void* volatile*>(native+slot),fn); };
            if(root) { patch(3,reinterpret_cast<void*>(&create)); patch(4,reinterpret_cast<void*>(&enumerate)); }
            else { patch(2,reinterpret_cast<void*>(&release)); patch(6,reinterpret_cast<void*>(&property)); patch(7,reinterpret_cast<void*>(&simple<7>)); patch(8,reinterpret_cast<void*>(&simple<8>));
#ifdef AC8_COMPAT
                patch(4,reinterpret_cast<void*>(&compatObjects)); patch(12,reinterpret_cast<void*>(&compatInfo));
#endif
                patch(9,reinterpret_cast<void*>(&state)); patch(10,reinterpret_cast<void*>(&data));
                patch(11,reinterpret_cast<void*>(&format)); patch(13,reinterpret_cast<void*>(&cooperative)); patch(25,reinterpret_cast<void*>(&simple<25>)); }
            DWORD unused; if(!VirtualProtect(native,(root?11:32)*sizeof(void*),old,&unused)) installed=false;
        }
    }
    if(h) {
        // Each explicit successful creation starts a new identity generation,
        // including address reuse after a native final Release.
        auto e=static_cast<Entry*>(HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Entry)));
        if(e) { e->object=object; e->root=root; e->wide=wide; e->guid=guid; e->alive=true;
            memcpy(e->original,h->original,sizeof(e->original)); e->next=entries; entries=e; }
        else installed=false;
    } else installed=false;
    ReleaseSRWLockExclusive(&entriesLock);
    log("Attach object=%p interface=%s encoding=%s installed=%d",object,root?"IDirectInput8":"IDirectInputDevice8",wide?"W":"A",installed);
}
BOOL CALLBACK initialize(PINIT_ONCE,void*,void**) {
    DWORD saved=GetLastError(); QueryPerformanceFrequency(&frequency);
    WCHAR system[MAX_PATH]{}; UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(n&&n<MAX_PATH-13) { wcscat_s(system,L"\\dinput8.dll"); real=LoadLibraryExW(system,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32); }
    WCHAR path[32768]{}; DWORD length=GetEnvironmentVariableW(L"AC8_DI_LOG",path,32768);
    if(!length||length>=32768) {
        DWORD k=GetModuleFileNameW(self,path,32768); if(k&&k<32768) {
            WCHAR* slash=wcsrchr(path,L'\\'); if(slash) {
                LARGE_INTEGER q; QueryPerformanceCounter(&q);
#ifdef AC8_COMPAT
                const wchar_t* pattern=L"ac8-compat-%lu-%lld.log";
#else
                const wchar_t* pattern=L"ac8-directinput-%lu-%lld.log";
#endif
                _snwprintf_s(slash+1,32768-(slash+1-path),_TRUNCATE,pattern,GetCurrentProcessId(),q.QuadPart);
            } else path[0]=0;
        } else path[0]=0;
    }
    if(path[0]) file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    log("Observer version=4 pid=%lu qpc_frequency=%lld exe_base=%p observer_base=%p real=%p system_path=%ls state_policy=change_or_2s poll_policy=change_or_256 writes=unbuffered_userspace",GetCurrentProcessId(),frequency.QuadPart,GetModuleHandleW(nullptr),self,real,system);
#ifdef AC8_COMPAT
    compat::initialize(self);
#else
    // Compatibility-only build: no profile parser instrumentation.
#endif
    SetLastError(saved); return TRUE;
}
FARPROC proc(const char* name) { InitOnceExecuteOnce(&once,initialize,nullptr,nullptr); return real?GetProcAddress(real,name):nullptr; }
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,void** out,IUnknown* outer) {
    DWORD before=GetLastError(); auto fn=reinterpret_cast<decltype(&DirectInput8Create)>(obs::proc("DirectInput8Create"));
    if(!fn) { SetLastError(ERROR_MOD_NOT_FOUND); return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND); }
    char id[40]; obs::guidText(iid,id); obs::log("DirectInput8Create enter version=%08lX iid=%s output_pointer=%p outer=%p",version,id,out,outer);
    SetLastError(before); HRESULT hr=fn(instance,version,iid,out,outer); DWORD after=GetLastError(); void* object=nullptr;
    if(SUCCEEDED(hr)&&!outer&&obs::copy(&object,out,sizeof(object))&&object&&(iid==IID_IDirectInput8W||iid==IID_IDirectInput8A))
        obs::attach(object,true,iid==IID_IDirectInput8W,GUID_NULL);
    obs::log("DirectInput8Create exit hr=%08lX object=%p",hr,object); SetLastError(after); return hr;
}
extern "C" HRESULT WINAPI DllCanUnloadNow() {
    // Forward the real result. AC8's static import retains this DLL for process
    // life; explicit unloading while patched methods exist is unsupported.
    DWORD saved=GetLastError(); auto fn=reinterpret_cast<HRESULT(WINAPI*)()>(obs::proc("DllCanUnloadNow")); SetLastError(saved);
    return fn?fn():HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void** out) {
    DWORD saved=GetLastError(); auto fn=reinterpret_cast<HRESULT(WINAPI*)(REFCLSID,REFIID,void**)>(obs::proc("DllGetClassObject")); SetLastError(saved);
    return fn?fn(clsid,iid,out):HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
}
extern "C" HRESULT WINAPI DllRegisterServer() { DWORD saved=GetLastError(); auto fn=reinterpret_cast<HRESULT(WINAPI*)()>(obs::proc("DllRegisterServer")); SetLastError(saved); return fn?fn():HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND); }
extern "C" HRESULT WINAPI DllUnregisterServer() { DWORD saved=GetLastError(); auto fn=reinterpret_cast<HRESULT(WINAPI*)()>(obs::proc("DllUnregisterServer")); SetLastError(saved); return fn?fn():HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND); }
extern "C" const DIDATAFORMAT* WINAPI GetdfDIJoystick() { DWORD saved=GetLastError(); auto fn=reinterpret_cast<const DIDATAFORMAT*(WINAPI*)()>(obs::proc("GetdfDIJoystick")); SetLastError(saved); return fn?fn():nullptr; }
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) { if(reason==DLL_PROCESS_ATTACH) { obs::self=instance; DisableThreadLibraryCalls(instance); } return TRUE; }
