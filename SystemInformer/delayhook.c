/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     dmex    2022-2023
 *
 */

#include <phapp.h>
#include <apiimport.h>

#include <vsstyle.h>

#include "settings.h"
#include "../tools/thirdparty/detours/detours.h"

// https://learn.microsoft.com/en-us/windows/win32/winmsg/about-window-procedures#window-procedure-superclassing
static WNDPROC PhDefaultMenuWindowProcedure = NULL;
static WNDPROC PhDefaultDialogWindowProcedure = NULL;
static WNDPROC PhDefaultComboBoxWindowProcedure = NULL;
static BOOLEAN PhDefaultEnableThemeSupport = FALSE;
static BOOLEAN PhDefaultEnableThemeAcrylicSupport = FALSE;
static BOOLEAN PhDefaultEnableThemeAcrylicWindowSupport = FALSE;
static BOOLEAN PhDefaultEnableStreamerMode = FALSE;

LRESULT CALLBACK PhMenuWindowHookProcedure(
    _In_ HWND WindowHandle,
    _In_ UINT WindowMessage,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (WindowMessage)
    {
    case WM_CREATE:
        {
            //CREATESTRUCT* createStruct = (CREATESTRUCT*)lParam;

            if (PhDefaultEnableStreamerMode)
            {
                if (SetWindowDisplayAffinity_Import())
                    SetWindowDisplayAffinity_Import()(WindowHandle, WDA_EXCLUDEFROMCAPTURE);
            }

            if (PhDefaultEnableThemeSupport)
            {
                HFONT fontHandle;
                LONG windowDpi = PhGetWindowDpi(WindowHandle);
                
                if (fontHandle = PhCreateMessageFont(windowDpi))
                {
                    PhSetWindowContext(WindowHandle, (ULONG)'font', fontHandle);
                    SetWindowFont(WindowHandle, fontHandle, TRUE);
                }

                if (PhDefaultEnableThemeAcrylicSupport)
                {
                    PhSetWindowAcrylicCompositionColor(WindowHandle, MakeABGRFromCOLORREF(0, RGB(10, 10, 10)));
                }
            }
        }
        break;
    case WM_DESTROY:
        {
            if (PhDefaultEnableThemeSupport)
            {
                HFONT fontHandle;

                fontHandle = PhGetWindowContext(WindowHandle, (ULONG)'font');
                PhRemoveWindowContext(WindowHandle, (ULONG)'font');

                if (fontHandle)
                {
                    DeleteFont(fontHandle);
                }
            }
        }
        break;
    }

    return PhDefaultMenuWindowProcedure(WindowHandle, WindowMessage, wParam, lParam);
}

LRESULT CALLBACK PhDialogWindowHookProcedure(
    _In_ HWND WindowHandle,
    _In_ UINT WindowMessage,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    switch (WindowMessage)
    {
    case WM_CREATE:
        {
            //CREATESTRUCT* createStruct = (CREATESTRUCT*)lParam;
            //IsTopLevelWindow(createStruct->hwndParent)

            if (WindowHandle == GetAncestor(WindowHandle, GA_ROOT))
            {
                if (PhDefaultEnableStreamerMode)
                {
                    if (SetWindowDisplayAffinity_Import())
                        SetWindowDisplayAffinity_Import()(WindowHandle, WDA_EXCLUDEFROMCAPTURE);
                }

                if (PhDefaultEnableThemeSupport && PhDefaultEnableThemeAcrylicWindowSupport)
                {
                    PhSetWindowAcrylicCompositionColor(WindowHandle, MakeABGRFromCOLORREF(0, RGB(10, 10, 10)));
                }
            }
        }
        break;
    }

    return PhDefaultDialogWindowProcedure(WindowHandle, WindowMessage, wParam, lParam);
}

LRESULT CALLBACK PhComboBoxWindowHookProcedure(
    _In_ HWND WindowHandle,
    _In_ UINT WindowMessage,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    )
{
    LRESULT result = PhDefaultComboBoxWindowProcedure(WindowHandle, WindowMessage, wParam, lParam);

    switch (WindowMessage)
    {
    case WM_CREATE:
        {
            //CREATESTRUCT* createStruct = (CREATESTRUCT*)lParam;
            COMBOBOXINFO info = { sizeof(COMBOBOXINFO) };

            if (SendMessage(WindowHandle, CB_GETCOMBOBOXINFO, 0, (LPARAM)&info))
            {
                if (PhDefaultEnableStreamerMode)
                {
                    if (SetWindowDisplayAffinity_Import())
                        SetWindowDisplayAffinity_Import()(info.hwndList, WDA_EXCLUDEFROMCAPTURE);
                }
            }
        }
        break;
    }

    return result;
}

VOID PhRegisterDialogSuperClass(
    VOID
    )
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };

    if (!GetClassInfoEx(NULL, L"#32770", &wcex))
        return;

    PhDefaultDialogWindowProcedure = wcex.lpfnWndProc;
    wcex.lpfnWndProc = PhDialogWindowHookProcedure;

    if (RegisterClassEx(&wcex) == INVALID_ATOM)
    {
        PhShowStatus(NULL, L"Unable to register window class.", 0, GetLastError());
    }
}

VOID PhRegisterMenuSuperClass(
    VOID
    )
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };

    if (!GetClassInfoEx(NULL, L"#32768", &wcex))
        return;

    PhDefaultMenuWindowProcedure = wcex.lpfnWndProc;
    wcex.lpfnWndProc = PhMenuWindowHookProcedure;

    if (RegisterClassEx(&wcex) == INVALID_ATOM)
    {
        PhShowStatus(NULL, L"Unable to register window class.", 0, GetLastError());
    }
}

VOID PhRegisterComboBoxSuperClass(
    VOID
    )
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };

    if (!GetClassInfoEx(NULL, L"ComboBox", &wcex))
        return;

    PhDefaultComboBoxWindowProcedure = wcex.lpfnWndProc;
    wcex.lpfnWndProc = PhComboBoxWindowHookProcedure;

    UnregisterClass(L"ComboBox", NULL); // Must be unregistered first? (dmex)

    if (RegisterClassEx(&wcex) == INVALID_ATOM)
    {
        PhShowStatus(NULL, L"Unable to register window class.", 0, GetLastError());
    }
}

// Detours export procedure hooks

static HRESULT (WINAPI* PhDefaultDrawThemeBackground)(
    _In_ HTHEME Theme,
    _In_ HDC Hdc,
    _In_ INT PartId,
    _In_ INT StateId,
    _In_ LPCRECT Rect,
    _In_ LPCRECT ClipRect
    ) = NULL;

//static HRESULT (WINAPI* PhDefaultDrawThemeBackgroundEx)(
//    _In_ HTHEME Theme,
//    _In_ HDC hdc,
//    _In_ INT PartId,
//    _In_ INT StateId,
//    _In_ LPCRECT Rect,
//    _In_ PVOID Options
//    ) = NULL;

static BOOL (WINAPI* PhDefaultSystemParametersInfo)(
    _In_ UINT uiAction,
    _In_ UINT uiParam,
    _Pre_maybenull_ _Post_valid_ PVOID pvParam,
    _In_ UINT fWinIni
    ) = NULL;

static HWND (WINAPI* PhDefaultCreateWindowEx)(
    _In_ ULONG ExStyle,
    _In_opt_ PCWSTR ClassName,
    _In_opt_ PCWSTR WindowName,
    _In_ ULONG Style,
    _In_ INT X,
    _In_ INT Y,
    _In_ INT Width,
    _In_ INT Height,
    _In_opt_ HWND Parent,
    _In_opt_ HMENU Menu,
    _In_opt_ PVOID Instance,
    _In_opt_ PVOID Param
    ) = NULL;

//static COLORREF (WINAPI* PhDefaultSetTextColor)(
//    _In_ HDC hdc, 
//    _In_ COLORREF color
//    ) = NULL;

HRESULT PhDrawThemeBackgroundHook(
    _In_ HTHEME Theme,
    _In_ HDC Hdc,
    _In_ INT PartId,
    _In_ INT StateId,
    _In_ LPCRECT Rect,
    _In_ LPCRECT ClipRect
    )
{
    if (WindowsVersion >= WINDOWS_11)
    {
        WCHAR className[MAX_PATH];

        if (PhGetThemeClass(Theme, className, RTL_NUMBER_OF(className)))
        {
            if (PhEqualStringZ(className, VSCLASS_MENU, TRUE))
            {
                if (PartId == MENU_POPUPGUTTER || PartId == MENU_POPUPBORDERS)
                {
                    FillRect(Hdc, Rect, PhThemeWindowBackgroundBrush);
                    return S_OK;
                }
            }
        }
    }

    return PhDefaultDrawThemeBackground(Theme, Hdc, PartId, StateId, Rect, ClipRect);
}

//HRESULT WINAPI PhDrawThemeBackgroundExHook(
//    _In_ HTHEME Theme,
//    _In_ HDC hdc,
//    _In_ INT PartId,
//    _In_ INT StateId,
//    _In_ LPCRECT Rect,
//    _In_ PVOID Options // DTBGOPTS
//    )
//{
//    //HWND windowHandle = WindowFromDC(hdc);
//    WCHAR className[MAX_PATH];
//
//    if (PhGetThemeClass(Theme, className, RTL_NUMBER_OF(className)))
//    {
//        if (!PhEqualStringZ(className, VSCLASS_TASKDIALOG, TRUE))
//            goto CleanupExit;
//    }
//
//    if (PartId == TDLG_PRIMARYPANEL && StateId == 0)
//    {
//        SetDCBrushColor(hdc, RGB(65, 65, 65));
//        FillRect(hdc, Rect, GetStockBrush(DC_BRUSH));
//        return S_OK;
//    }
//
//    if (PartId == TDLG_SECONDARYPANEL && StateId == 0)
//    {
//        SetDCBrushColor(hdc, RGB(45, 45, 45));
//        FillRect(hdc, Rect, GetStockBrush(DC_BRUSH));
//        return S_OK;
//    }
//
//CleanupExit:
//    return PhDefaultDrawThemeBackgroundEx(Theme, hdc, PartId, StateId, Rect, Options);
//}

HWND PhCreateWindowExHook(
    _In_ ULONG ExStyle,
    _In_opt_ PCWSTR ClassName,
    _In_opt_ PCWSTR WindowName,
    _In_ ULONG Style,
    _In_ INT X,
    _In_ INT Y,
    _In_ INT Width,
    _In_ INT Height,
    _In_opt_ HWND Parent,
    _In_opt_ HMENU Menu,
    _In_opt_ PVOID Instance,
    _In_opt_ PVOID Param
    )
{
    HWND windowHandle = PhDefaultCreateWindowEx(
        ExStyle, 
        ClassName, 
        WindowName,
        Style,
        X,
        Y, 
        Width, 
        Height,
        Parent,
        Menu, 
        Instance, 
        Param
        );

    if (Parent == NULL)
    {
        if (PhDefaultEnableStreamerMode)
        {
            if (SetWindowDisplayAffinity_Import())
                SetWindowDisplayAffinity_Import()(windowHandle, WDA_EXCLUDEFROMCAPTURE);
        }

        if (PhDefaultEnableThemeSupport && PhDefaultEnableThemeAcrylicWindowSupport)
        {
            PhSetWindowAcrylicCompositionColor(windowHandle, MakeABGRFromCOLORREF(0, RGB(10, 10, 10)));
        }
    }
    else 
    {
        //HWND parentHandle;
        //
        //if (parentHandle = GetAncestor(windowHandle, GA_ROOT))
        //{
        //    if (!IS_INTRESOURCE(ClassName) && PhEqualStringZ((PWSTR)ClassName, L"DirectUIHWND", TRUE))
        //    {
        //        PhInitializeTaskDialogTheme(windowHandle, PhDefaultThemeSupport); // don't initialize parentHandle themes
        //        PhInitializeThemeWindowFrame(parentHandle);
        //
        //        if (PhDefaultEnableStreamerMode)
        //        {
        //            if (SetWindowDisplayAffinity_Import())
        //                SetWindowDisplayAffinity_Import()(parentHandle, WDA_EXCLUDEFROMCAPTURE);
        //        }
        //    }
        //}
    }

    return windowHandle;
}

BOOL WINAPI PhSystemParametersInfoHook(
    _In_ UINT uiAction,
    _In_ UINT uiParam,
    _Pre_maybenull_ _Post_valid_ PVOID pvParam,
    _In_ UINT fWinIni
    )
{
    if (uiAction == SPI_GETMENUFADE && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETCLIENTAREAANIMATION && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETCOMBOBOXANIMATION && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETTOOLTIPANIMATION && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETMENUANIMATION && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETTOOLTIPFADE && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    if (uiAction == SPI_GETMOUSEVANISH && pvParam)
    {
        *((PBOOL)pvParam) = FALSE;
        return TRUE;
    }

    return PhDefaultSystemParametersInfo(uiAction, uiParam, pvParam, fWinIni);
}

//ULONG WINAPI GetSysColorHook(int nIndex)
//{
//    if (nIndex == COLOR_WINDOW)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_MENUTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_WINDOWTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_CAPTIONTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_HIGHLIGHTTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_GRAYTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_BTNTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_INACTIVECAPTIONTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_BTNFACE)
//        return PhThemeWindowBackgroundColor;
//    if (nIndex == COLOR_BTNTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_BTNHIGHLIGHT)
//        return PhThemeWindowBackgroundColor;
//    if (nIndex == COLOR_INFOTEXT)
//        return PhThemeWindowTextColor;
//    if (nIndex == COLOR_INFOBK)
//        return PhThemeWindowBackgroundColor;
//    if (nIndex == COLOR_MENU)
//        return PhThemeWindowBackgroundColor;
//    if (nIndex == COLOR_HIGHLIGHT)
//        return PhThemeWindowForegroundColor;
//    return originalGetSysColor(nIndex);
//}
//
//HBRUSH WINAPI GetSysColorBrushHook(_In_ int nIndex)
//{
//    //if (nIndex == COLOR_WINDOW)
//    //   return originalCreateSolidBrush(PhThemeWindowBackgroundColor);
//    if (nIndex == COLOR_BTNFACE)
//        return originalCreateSolidBrush(PhThemeWindowBackgroundColor);
//    return originalHook(nIndex);
//}
//
// RGB(GetBValue(color), GetGValue(color), GetRValue(color));
//#define RGB_FROM_COLOREF(cref) \
//    ((((cref) & 0x000000FF) << 16) | (((cref) & 0x0000FF00)) | (((cref) & 0x00FF0000) >> 16))
//
//COLORREF WINAPI PhSetTextColorHook(
//    _In_ HDC hdc, 
//    _In_ COLORREF color
//    )
//{
//    //HWND windowHandle = WindowFromDC(hdc);
//
//    if (!(PhTaskDialogThemeHookIndex && TlsGetValue(PhTaskDialogThemeHookIndex)))
//        goto CleanupExit;
//
//    if (RGB_FROM_COLOREF(RGB_FROM_COLOREF(color)) == RGB(0, 51, 153)) // TMT_TEXTCOLOR (TaskDialog InstructionPane)
//    {
//        color = RGB(0, 102, 255);
//    }
//    else if (RGB_FROM_COLOREF(RGB_FROM_COLOREF(color)) == RGB(0, 102, 204)) // TaskDialog Link
//    {
//        color = RGB(0, 128, 0); // RGB(128, 255, 128);
//    }
//    else
//    {
//        color = RGB(255, 255, 255); // GetBkColor(hdc);
//    }
//
//CleanupExit:
//    return PhDefaultSetTextColor(hdc, color);
//}

VOID PhRegisterDetoursHooks(
    VOID
    )
{
    NTSTATUS status;
    PVOID baseAddress;

    //if (baseAddress = PhGetLoaderEntryDllBase(L"gdi32.dll"))
    //{
    //    PhDefaultSetTextColor = PhGetDllBaseProcedureAddress(baseAddress, "SetTextColor", 0);
    //}

    if (baseAddress = PhGetLoaderEntryDllBase(L"user32.dll"))
    {
        PhDefaultCreateWindowEx = PhGetDllBaseProcedureAddress(baseAddress, "CreateWindowExW", 0);
        PhDefaultSystemParametersInfo = PhGetDllBaseProcedureAddress(baseAddress, "SystemParametersInfoW", 0);
    }

    if (baseAddress = PhGetLoaderEntryDllBase(L"uxtheme.dll"))
    {
        PhDefaultDrawThemeBackground = PhGetDllBaseProcedureAddress(baseAddress, "DrawThemeBackground", 0);
        //PhDefaultDrawThemeBackgroundEx = PhGetDllBaseProcedureAddress(baseAddress, "DrawThemeBackgroundEx", 0);
    }

    if (!NT_SUCCESS(status = DetourTransactionBegin()))
        goto CleanupExit;

    if (PhDefaultEnableThemeSupport || PhDefaultEnableThemeAcrylicSupport)
    {
        if (!NT_SUCCESS(status = DetourAttach((PVOID)&PhDefaultDrawThemeBackground, (PVOID)PhDrawThemeBackgroundHook)))
            goto CleanupExit;
        //if (!NT_SUCCESS(status = DetourAttach((PVOID)&PhDefaultDrawThemeBackgroundEx, (PVOID)PhDrawThemeBackgroundExHook)))
        //    goto CleanupExit;
        if (!NT_SUCCESS(status = DetourAttach((PVOID)&PhDefaultSystemParametersInfo, (PVOID)PhSystemParametersInfoHook)))
            goto CleanupExit;
    }

    if (!NT_SUCCESS(status = DetourAttach((PVOID)&PhDefaultCreateWindowEx, (PVOID)PhCreateWindowExHook)))
        goto CleanupExit;
    //if (!NT_SUCCESS(status = DetourAttach((PVOID)&PhDefaultSetTextColor, (PVOID)PhSetTextColorHook)))
    //    goto CleanupExit;
    if (!NT_SUCCESS(status = DetourTransactionCommit()))
        goto CleanupExit;

CleanupExit:

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(NULL, L"Unable to commit detours transaction.", status, 0);
    }
}

VOID PhInitializeSuperclassControls(
    VOID
    )
{
    PhDefaultEnableThemeSupport = !!PhGetIntegerSetting(L"EnableThemeSupport");
    PhDefaultEnableStreamerMode = !!PhGetIntegerSetting(L"EnableStreamerMode");

    if (PhDefaultEnableThemeSupport || PhDefaultEnableStreamerMode)
    {
        if (WindowsVersion >= WINDOWS_11)
        {
            PhDefaultEnableThemeAcrylicSupport = !!PhGetIntegerSetting(L"EnableThemeAcrylicSupport");
            PhDefaultEnableThemeAcrylicWindowSupport = !!PhGetIntegerSetting(L"EnableThemeAcrylicWindowSupport");
        }
    }

    if (PhDefaultEnableThemeSupport || PhDefaultEnableStreamerMode)
    {
        PhRegisterDialogSuperClass();
        PhRegisterMenuSuperClass();
        PhRegisterComboBoxSuperClass();

        PhRegisterDetoursHooks();
    }
}
