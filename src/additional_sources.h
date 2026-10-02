// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
// Included inside namespace compat. Owns only additional input devices.
// All native calls bypass shared vtable hooks; no worker or DllMain work.
struct OwnedSource {
    void* object=nullptr;void* methods[32]{};
    unsigned axes=0,buttons=0;unsigned char present[128]{};
    DIOBJECTDATAFORMAT formatObjects[134]{};DWORD formatCount=0;
    bool acquired=false,valid=false,sampled=false;
    DIJOYSTATE2 state{},lastLogged{};bool logged=false;ULONGLONG lastLog=0;
    ULONGLONG nextAttempt=0,lastSample=0;
    HRESULT lastStatus=E_PENDING;
};
OwnedSource owned[maxSources];void* sourceRoot=nullptr;void* rootMethods[11]{};
SRWLOCK extraLock=SRWLOCK_INIT;HWND inputWindow=nullptr;
bool nativeMethods(void* object,void** methods,size_t count) {
    void** table=nullptr;if(!obs::copy(&table,object,sizeof(table))) return false;
    AcquireSRWLockShared(&obs::entriesLock);
    bool copied=false;
    for(obs::Hooks* h=obs::tables;h;h=h->next) if(h->native==table) {
        memcpy(methods,h->original,count*sizeof(void*));copied=true;break;
    }
    if(!copied) copied=obs::copy(methods,table,count*sizeof(void*));
    ReleaseSRWLockShared(&obs::entriesLock);return copied;
}
void unacquireOwned(OwnedSource& source) {
    if(source.object&&source.acquired) reinterpret_cast<obs::Simple>(source.methods[8])(source.object);
    source.acquired=false;source.valid=false;source.sampled=false;source.logged=false;source.state=DIJOYSTATE2{};
}
void closeOwned(OwnedSource& source) {
    unacquireOwned(source);
    if(source.object) {
        void* object=source.object;
        reinterpret_cast<obs::Release>(source.methods[2])(object);
        // A native implementation may have called a hooked virtual method
        // internally. Retire any lazily created observer record before reuse.
        AcquireSRWLockExclusive(&obs::entriesLock);
        for(obs::Entry* e=obs::entries;e;e=e->next) if(e->object==object) e->alive=false;
        ReleaseSRWLockExclusive(&obs::entriesLock);
    }
    source=OwnedSource{};
}
void suspendExtras() {
    if(!hasAdditionalSources) return;
    DWORD saved=GetLastError();AcquireSRWLockExclusive(&extraLock);
    for(int i=2;i<maxSources;++i) unacquireOwned(owned[i]);
    ReleaseSRWLockExclusive(&extraLock);SetLastError(saved);
}
void stopExtras() {
    DWORD saved=GetLastError();AcquireSRWLockExclusive(&extraLock);
    for(auto& source:owned) closeOwned(source);
        if(sourceRoot) {
        reinterpret_cast<obs::Release>(rootMethods[2])(sourceRoot);
        AcquireSRWLockExclusive(&obs::entriesLock);
        for(obs::Entry* e=obs::entries;e;e=e->next) if(e->object==sourceRoot) e->alive=false;
        ReleaseSRWLockExclusive(&obs::entriesLock);
    }
    sourceRoot=nullptr;
    ReleaseSRWLockExclusive(&extraLock);SetLastError(saved);
}
void gameWindow(HWND window) {
    if(window&&!hasAdditionalSources) return;
    DWORD saved=GetLastError(),process=0;
    GetWindowThreadProcessId(window,&process);
    if(!window||process==GetCurrentProcessId()) {
        HWND root=window?GetAncestor(window,GA_ROOT):nullptr;AcquireSRWLockExclusive(&extraLock);
        if(inputWindow!=root) {
            for(auto& source:owned) closeOwned(source);
            inputWindow=root;
        }
        ReleaseSRWLockExclusive(&extraLock);
    }
    SetLastError(saved);
}
BOOL CALLBACK sourceObject(const DIDEVICEOBJECTINSTANCEW* object,void* context) {
    auto& source=*static_cast<OwnedSource*>(context);
    if(object->dwType&DIDFT_ABSAXIS) {
        unsigned a=axisId(object->guidType);
        if(a<6&&!(source.axes&(1u<<a))) {
            source.axes|=1u<<a;
            source.formatObjects[source.formatCount++]={axisGuids[a],a*4,
                (object->dwType&DIDFT_INSTANCEMASK)|DIDFT_ABSAXIS,DIDOI_ASPECTPOSITION};
        }
    }
    if(object->dwType&DIDFT_BUTTON) {
        unsigned b=DIDFT_GETINSTANCE(object->dwType);
        if(b<128&&!source.present[b]) {
            source.present[b]=1;if(b+1>source.buttons) source.buttons=b+1;
            source.formatObjects[source.formatCount++]={&GUID_Button,DIJOFS_BUTTON(b),
                (object->dwType&DIDFT_INSTANCEMASK)|DIDFT_BUTTON,0};
        }
    }
    return DIENUM_CONTINUE;
}
bool supportsSource(int index,const OwnedSource& source) {
    for(size_t i=0;i<routeCount;++i) if(bindings[i].source==index&&bindings[i].index>=0) {
        unsigned channel=static_cast<unsigned>(bindings[i].index);
        if(routes[i].axis?!(source.axes&(1u<<channel)):!source.present[channel]) return false;
    }
    return true;
}
HRESULT setupSource(int index,HWND window) {
    auto& source=owned[index];
    if(!sourceRoot) {
        auto create=reinterpret_cast<decltype(&DirectInput8Create)>(GetProcAddress(obs::real,"DirectInput8Create"));
        if(!create) return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
        HRESULT hr=create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,&sourceRoot,nullptr);
        if(FAILED(hr)) {sourceRoot=nullptr;return hr;}
        if(!nativeMethods(sourceRoot,rootMethods,11)) {
            // Native COM object exists but cannot be safely inspected. Do not
            // dereference unknown pointers to attempt cleanup; process owns it.
            sourceRoot=nullptr;return E_FAIL;
        }
    }
    HRESULT hr=reinterpret_cast<obs::Create>(rootMethods[3])(sourceRoot,sources[index].instance,&source.object,nullptr);
    if(FAILED(hr)) {source.object=nullptr;return hr;}
    if(!nativeMethods(source.object,source.methods,32)) {source.object=nullptr;return E_FAIL;}
    hr=reinterpret_cast<obs::Objects>(source.methods[4])(source.object,reinterpret_cast<void*>(&sourceObject),&source,DIDFT_ALL);
    if(FAILED(hr)||!supportsSource(index,source)) return FAILED(hr)?hr:DIERR_OBJECTNOTFOUND;
    // Map only enumerated objects into the public DIJOYSTATE2 offsets. No
    // optional-object flags, guessed instance IDs, or copied SDK data tables.
    DIDATAFORMAT format{sizeof(DIDATAFORMAT),sizeof(DIOBJECTDATAFORMAT),DIDF_ABSAXIS,
        sizeof(DIJOYSTATE2),source.formatCount,source.formatObjects};
    hr=reinterpret_cast<obs::Format>(source.methods[11])(source.object,&format);if(FAILED(hr)) return hr;
    unsigned requiredAxes=0;
    for(size_t i=0;i<routeCount;++i) if(bindings[i].source==index&&routes[i].axis&&bindings[i].index>=0)
        requiredAxes|=1u<<bindings[i].index;
    for(unsigned a=0;a<6;++a) if(requiredAxes&(1u<<a)) {
        DIPROPRANGE range{{sizeof(DIPROPRANGE),sizeof(DIPROPHEADER),a*4,DIPH_BYOFFSET},-1000,1000};
        hr=reinterpret_cast<obs::Property>(source.methods[6])(source.object,DIPROP_RANGE,&range.diph);
        if(FAILED(hr)) return hr;
    }
    return reinterpret_cast<obs::Coop>(source.methods[13])(source.object,window,DISCL_FOREGROUND|DISCL_NONEXCLUSIVE);
}
HRESULT readOwned(OwnedSource& source) {
    source.valid=false;source.state=DIJOYSTATE2{};
    HRESULT hr=E_FAIL;
    for(int attempt=0;attempt<2;++attempt) {
        if(!source.acquired) {
            hr=reinterpret_cast<obs::Simple>(source.methods[7])(source.object);
            if(FAILED(hr)) return hr;
            source.acquired=true;
        }
        hr=reinterpret_cast<obs::Simple>(source.methods[25])(source.object);
        if(SUCCEEDED(hr)) {
            DIJOYSTATE2 state{};
            hr=reinterpret_cast<obs::State>(source.methods[9])(source.object,sizeof(state),&state);
            if(SUCCEEDED(hr)) {source.state=state;source.valid=true;return hr;}
        }
        if(hr!=DIERR_INPUTLOST&&hr!=DIERR_NOTACQUIRED) return hr;
        source.acquired=false;
    }
    return hr;
}
void sampleSources(bool foreground,HWND window,ULONGLONG now,DIJOYSTATE2* states,bool* valid) {
    // Caller holds extraLock. Bound work to one read/reacquisition sequence per
    // source, and at most one creation attempt per second for absent devices.
    for(int i=2;i<sourceCount;++i) if(sources[i].used) {
        auto& source=owned[i];
        if(!foreground) {
            unacquireOwned(source);
            if(source.lastStatus!=DIERR_NOTACQUIRED) {
                obs::log("AdditionalSource name=%ls ready=0 hr=%08lX reason=game_not_foreground",sources[i].name,DIERR_NOTACQUIRED);
                source.lastStatus=DIERR_NOTACQUIRED;
            }
            continue;
        }
        if(source.sampled&&now-source.lastSample<2) {states[i]=source.state;valid[i]=source.valid;continue;}
        HRESULT hr=E_PENDING;
        if(!source.object&&now>=source.nextAttempt) {
            hr=setupSource(i,window);
            if(FAILED(hr)) {closeOwned(source);source.nextAttempt=now+1000;}
        }
        if(source.object) hr=readOwned(source);
        if(hr==DIERR_UNPLUGGED) {closeOwned(source);source.nextAttempt=now+1000;}
        if(hr!=source.lastStatus) {
            obs::log("AdditionalSource name=%ls ready=%d hr=%08lX axis_mask=%02X buttons=%u",sources[i].name,source.valid,hr,source.axes,source.buttons);
            source.lastStatus=hr;
        }
        if(source.valid&&(!source.logged||memcmp(&source.state,&source.lastLogged,sizeof(source.state))||now-source.lastLog>=2000)) {
            char buttons[1024]="";size_t used=0;
            for(unsigned b=0;b<128;++b) if(source.state.rgbButtons[b]&0x80)
                used+=sprintf_s(buttons+used,sizeof(buttons)-used,"%u,",b+1);
            obs::log("AdditionalState name=%ls axes=%ld,%ld,%ld,%ld,%ld,%ld pressed_buttons_one_based=%s",
                sources[i].name,source.state.lX,source.state.lY,source.state.lZ,source.state.lRx,source.state.lRy,source.state.lRz,buttons);
            source.lastLogged=source.state;source.logged=true;source.lastLog=now;
        }
        source.sampled=true;source.lastSample=now;
        states[i]=source.state;valid[i]=source.valid;
    }
}
void merge(int r,const DIJOYSTATE2& input,DIJOYSTATE2& output) {
    if(!hasAdditionalSources) {translate(r,input,output);return;}
    DWORD saved=GetLastError();DIJOYSTATE2 states[maxSources]{};bool valid[maxSources]{};
    AcquireSRWLockExclusive(&extraLock);
    HWND foreground=GetForegroundWindow();DWORD process=0;GetWindowThreadProcessId(inputWindow,&process);
    bool focused=inputWindow&&IsWindow(inputWindow)&&process==GetCurrentProcessId()&&GetAncestor(foreground,GA_ROOT)==inputWindow;
    sampleSources(focused,inputWindow,GetTickCount64(),states,valid);
    ReleaseSRWLockExclusive(&extraLock);
    translateSources(r,input,states,valid,output);SetLastError(saved);
}
