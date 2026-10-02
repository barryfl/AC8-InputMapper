// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
// Standalone, read-only DirectInput inventory. No acquisition or state reads.
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <objbase.h>
#include <cstdio>
#include <cstdarg>
FILE* report;
void print(const wchar_t* fmt,...) {
    wchar_t buffer[2048];va_list args;va_start(args,fmt);_vsnwprintf_s(buffer,_TRUNCATE,fmt,args);va_end(args);
    DWORD written;HANDLE output=GetStdHandle(STD_OUTPUT_HANDLE);
    if(!WriteConsoleW(output,buffer,static_cast<DWORD>(wcslen(buffer)),&written,nullptr)) {
        char utf8[8192];int size=WideCharToMultiByte(CP_UTF8,0,buffer,-1,utf8,sizeof(utf8),nullptr,nullptr);
        if(size>1) WriteFile(output,utf8,static_cast<DWORD>(size-1),&written,nullptr);
    }
    if(report) {fputws(buffer,report);fflush(report);}
}
struct Inventory {IDirectInput8W* di; unsigned count; bool failed;};
BOOL CALLBACK controller(const DIDEVICEINSTANCEW* d,void* context) {
    auto& inventory=*static_cast<Inventory*>(context);++inventory.count;
    wchar_t instance[40],product[40];StringFromGUID2(d->guidInstance,instance,40);StringFromGUID2(d->guidProduct,product,40);
    print(L"\nController %u: %ls\nInstanceGUID=%ls\nProductGUID=%ls\n",inventory.count,d->tszProductName,instance,product);
    IDirectInputDevice8W* device=nullptr;HRESULT hr=inventory.di->CreateDevice(d->guidInstance,&device,nullptr);
    if(FAILED(hr)) {print(L"Details unavailable: CreateDevice HRESULT=%08lX\n",hr);inventory.failed=true;return DIENUM_CONTINUE;}
    DIDEVCAPS caps{};caps.dwSize=sizeof(caps);hr=device->GetCapabilities(&caps);
    if(SUCCEEDED(hr)) print(L"Buttons=%lu  NativePOVs=%lu  Axes=%lu\n",caps.dwButtons,caps.dwPOVs,caps.dwAxes);
    else {print(L"Capabilities unavailable HRESULT=%08lX\n",hr);inventory.failed=true;}
    hr=device->SetDataFormat(&c_dfDIJoystick2);
    if(SUCCEEDED(hr)) {
        const DWORD offsets[]={DIJOFS_X,DIJOFS_Y,DIJOFS_Z,DIJOFS_RX,DIJOFS_RY,DIJOFS_RZ,DIJOFS_SLIDER(0),DIJOFS_SLIDER(1)};
        const wchar_t* names[]={L"X",L"Y",L"Z",L"Rx",L"Ry",L"Rz",L"Slider1",L"Slider2"};
        print(L"AvailableAxes=");
        for(unsigned i=0;i<8;++i) {DIDEVICEOBJECTINSTANCEW object{};object.dwSize=sizeof(object);
            if(SUCCEEDED(device->GetObjectInfo(&object,offsets[i],DIPH_BYOFFSET))) print(L"%ls ",names[i]);}
        print(L"\n(Sliders and native POV bindings are not supported by compatibility version 1.)\n");
    } else {print(L"Axis inventory unavailable: SetDataFormat HRESULT=%08lX\n",hr);inventory.failed=true;}
    device->Release();return DIENUM_CONTINUE;
}
int wmain() {
    wchar_t path[32768];DWORD length=GetModuleFileNameW(nullptr,path,32768);
    if(!length||length>=32768) return 1;
    wchar_t* slash=wcsrchr(path,L'\\');if(!slash) return 1;
    wcscpy_s(slash+1,32768-static_cast<size_t>(slash+1-path),L"controllers.txt");
    _wfopen_s(&report,path,L"wt, ccs=UTF-8");
    print(L"AC8 controller inventory - copy InstanceGUID, not ProductGUID.\n");
    wchar_t system[MAX_PATH];UINT n=GetSystemDirectoryW(system,MAX_PATH);
    if(!n||n>=MAX_PATH-13) return 1;
    wcscat_s(system,L"\\dinput8.dll");HMODULE module=LoadLibraryExW(system,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) {print(L"Cannot load system DirectInput. Error=%lu\n",GetLastError());if(report) fclose(report);return 1;}
    auto make=reinterpret_cast<decltype(&DirectInput8Create)>(GetProcAddress(module,"DirectInput8Create"));
    IDirectInput8W* di=nullptr;HRESULT hr=make?make(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void**>(&di),nullptr):E_FAIL;
    Inventory inventory{di,0,false};
    if(SUCCEEDED(hr)&&di) {hr=di->EnumDevices(DI8DEVCLASS_GAMECTRL,controller,&inventory,DIEDFL_ATTACHEDONLY);di->Release();}
    if(FAILED(hr)) print(L"Enumeration failed HRESULT=%08lX\n",hr);
    else print(L"\nConnected controllers: %u\n",inventory.count);
    print(L"\nReport saved beside this tool: controllers.txt\nNo game files were read or modified. No device was acquired or polled.\n");
    if(report) fclose(report);FreeLibrary(module);return FAILED(hr)||inventory.failed?1:0;
}
