// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Fred Barry
#pragma once
namespace compat {
void initialize(HMODULE);
int role(REFGUID);
void identity(DIDEVICEINSTANCEW&);
void identity(DIDEVICEINSTANCEA&);
void objectIdentity(int,DIDEVICEOBJECTINSTANCEW&,DWORD* =nullptr);
void objectIdentity(int,DIDEVICEOBJECTINSTANCEA&,DWORD* =nullptr);
bool caller(void*,DWORD);
void translate(int,const DIJOYSTATE2&,DIJOYSTATE2&);
void merge(int,const DIJOYSTATE2&,DIJOYSTATE2&);
void gameWindow(HWND);
void suspendExtras();
void stopExtras();
bool additional(REFGUID);
bool externalOutputsReady(int,DWORD);
bool enabled();
void sourceLog(int,const DIJOYSTATE2&);
unsigned axisId(REFGUID);
bool supported(int,unsigned,unsigned,const unsigned char* =nullptr);
}
