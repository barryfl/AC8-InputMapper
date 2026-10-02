// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
// Included inside namespace obs, in the compatibility build only.
using Objects = HRESULT (STDMETHODCALLTYPE*)(void*,void*,void*,DWORD);
using Info = HRESULT (STDMETHODCALLTYPE*)(void*,void*);
struct ObjectContext {
    void* callback; void* context; int role; unsigned axes,buttons; bool continued=true,hat=false; DWORD advertised=0;
    unsigned char present[128]{};
};
template<class T> void observedObject(ObjectContext& c,const T& d) {
    if(d.dwType&DIDFT_AXIS) { unsigned a=compat::axisId(d.guidType); if(a<8) c.axes|=1u<<a; }
    if(d.dwType&DIDFT_BUTTON) { unsigned n=DIDFT_GETINSTANCE(d.dwType)+1; if(n>c.buttons) c.buttons=n; if(n<=128) c.present[n-1]=1; }
    if(d.dwType&DIDFT_POV) c.hat=true;
}
BOOL CALLBACK compatObjectW(const DIDEVICEOBJECTINSTANCEW* p,void* context) {
    auto& c=*static_cast<ObjectContext*>(context); DWORD saved=GetLastError(); DIDEVICEOBJECTINSTANCEW d{};
    const DIDEVICEOBJECTINSTANCEW* out=p;
    if(copy(&d,p,sizeof(d))) { observedObject(c,d); compat::objectIdentity(c.role,d,&c.advertised); out=&d; }
    SetLastError(saved); BOOL result=reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKW>(c.callback)(out,c.context);
    c.continued=result!=DIENUM_STOP; return result;
}
BOOL CALLBACK compatObjectA(const DIDEVICEOBJECTINSTANCEA* p,void* context) {
    auto& c=*static_cast<ObjectContext*>(context); DWORD saved=GetLastError(); DIDEVICEOBJECTINSTANCEA d{};
    const DIDEVICEOBJECTINSTANCEA* out=p;
    if(copy(&d,p,sizeof(d))) { observedObject(c,d); compat::objectIdentity(c.role,d,&c.advertised); out=&d; }
    SetLastError(saved); BOOL result=reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKA>(c.callback)(out,c.context);
    c.continued=result!=DIENUM_STOP; return result;
}
HRESULT STDMETHODCALLTYPE compatObjects(void* o,void* callback,void* context,DWORD flags) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Objects>(e->original[4]);
    int r=compat::role(e->guid);
    bool active=r&&callback&&flags==DIDFT_ALL&&compat::caller(_ReturnAddress(),0x4E696A1);
    ObjectContext c{callback,context,r,0,0};
    void* hook=e->wide?reinterpret_cast<void*>(&compatObjectW):reinterpret_cast<void*>(&compatObjectA);
    SetLastError(before); HRESULT hr=fn(o,active?hook:callback,active?&c:context,flags); DWORD after=GetLastError();
    if(active) {
        bool ready=SUCCEEDED(hr)&&c.continued&&compat::supported(r,c.axes,c.buttons,c.present)&&compat::externalOutputsReady(r,c.advertised);
        e->compatibilityObjects=ready;
        if(ready&&!c.hat) {
            // No driver call for this synthesized object. The game's callback
            // only sets capability bits for POV objects; range calls are axes only.
            SetLastError(after);
            if(e->wide) { DIDEVICEOBJECTINSTANCEW d{}; d.dwSize=sizeof(d); d.guidType=GUID_POV;
                d.dwType=DIDFT_POV|DIDFT_MAKEINSTANCE(0); d.dwOfs=DIJOFS_POV(0); wcscpy_s(d.tszName,L"AC8 mapped hat");
                reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKW>(callback)(&d,context);
            } else { DIDEVICEOBJECTINSTANCEA d{}; d.dwSize=sizeof(d); d.guidType=GUID_POV;
                d.dwType=DIDFT_POV|DIDFT_MAKEINSTANCE(0); d.dwOfs=DIJOFS_POV(0); strcpy_s(d.tszName,"AC8 mapped hat");
                reinterpret_cast<LPDIENUMDEVICEOBJECTSCALLBACKA>(callback)(&d,context);
            }
        }
        log("Compatibility objects device=%p role=%d source_axis_mask=%02X source_buttons=%u ready=%d synthetic_hat=%d hr=%08lX",o,r,c.axes,c.buttons,ready,ready&&!c.hat,hr);
    }
    SetLastError(after); return hr;
}
HRESULT STDMETHODCALLTYPE compatInfo(void* o,void* p) {
    DWORD before=GetLastError(); Entry* e=find(o); auto fn=reinterpret_cast<Info>(e->original[12]);
    SetLastError(before); HRESULT hr=fn(o,p); DWORD after=GetLastError();
    if(SUCCEEDED(hr)&&compat::role(e->guid)) {
        if(e->wide) { DIDEVICEINSTANCEW d{}; if(copy(&d,p,sizeof(d))&&d.dwSize==sizeof(d)) {compat::identity(d);memcpy(p,&d,sizeof(d));} }
        else { DIDEVICEINSTANCEA d{}; if(copy(&d,p,sizeof(d))&&d.dwSize==sizeof(d)) {compat::identity(d);memcpy(p,&d,sizeof(d));} }
    }
    SetLastError(after); return hr;
}
