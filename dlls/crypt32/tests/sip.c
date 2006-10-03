/*
 * Subject Interface Package tests
 *
 * Copyright 2006 Paul Vriens
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdio.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <winnls.h>
#include <wincrypt.h>
#include <mssip.h>

#include "wine/test.h"

static BOOL (WINAPI * funcCryptSIPGetSignedDataMsg)(SIP_SUBJECTINFO *,DWORD *,DWORD,DWORD *,BYTE *);
static BOOL (WINAPI * funcCryptSIPPutSignedDataMsg)(SIP_SUBJECTINFO *,DWORD,DWORD *,DWORD,BYTE *);
static BOOL (WINAPI * funcCryptSIPCreateIndirectData)(SIP_SUBJECTINFO *,DWORD *,SIP_INDIRECT_DATA *);
static BOOL (WINAPI * funcCryptSIPVerifyIndirectData)(SIP_SUBJECTINFO *,SIP_INDIRECT_DATA *);
static BOOL (WINAPI * funcCryptSIPRemoveSignedDataMsg)(SIP_SUBJECTINFO *,DWORD);

static char *show_guid(const GUID *guid)
{
    static char guidstring[39];

    sprintf(guidstring,
        "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        guid->Data1, guid->Data2, guid->Data3,
        guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
        guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7] );

    return guidstring;
}

static void test_AddRemoveProvider(void)
{
    BOOL ret;
    SIP_ADD_NEWPROVIDER newprov;
    GUID actionid = { 0xdeadbe, 0xefde, 0xadbe, { 0xef,0xde,0xad,0xbe,0xef,0xde,0xad,0xbe }};
    static WCHAR dummydll[]      = {'d','e','a','d','b','e','e','f','.','d','l','l',0 };
    static WCHAR dummyfunction[] = {'d','u','m','m','y','f','u','n','c','t','i','o','n',0 };

    /* NULL check */
    SetLastError(0xdeadbeef);
    ret = CryptSIPRemoveProvider(NULL);
    ok (!ret, "Expected CryptSIPRemoveProvider to fail.\n");
    ok (GetLastError() == ERROR_INVALID_PARAMETER,
        "Expected ERROR_INVALID_PARAMETER, got %d.\n", GetLastError());

    /* nonexistent provider should result in a registry error */
    SetLastError(0xdeadbeef);
    ret = CryptSIPRemoveProvider(&actionid);
    ok (!ret, "Expected CryptSIPRemoveProvider to fail.\n");
    ok (GetLastError() == ERROR_FILE_NOT_FOUND,
        "Expected ERROR_FILE_NOT_FOUND, got %d.\n", GetLastError());

    /* Everything OK, pwszIsFunctionName and pwszIsFunctionNameFmt2 are left NULL
     * as allowed */

    memset(&newprov, 0, sizeof(SIP_ADD_NEWPROVIDER));
    newprov.cbStruct = sizeof(SIP_ADD_NEWPROVIDER);
    newprov.pgSubject = &actionid;
    newprov.pwszDLLFileName = dummydll;
    newprov.pwszGetFuncName = dummyfunction;
    newprov.pwszPutFuncName = dummyfunction;
    newprov.pwszCreateFuncName = dummyfunction;
    newprov.pwszVerifyFuncName = dummyfunction;
    newprov.pwszRemoveFuncName = dummyfunction;
    SetLastError(0xdeadbeef);
    ret = CryptSIPAddProvider(&newprov);
    ok ( ret, "CryptSIPAddProvider should have succeeded\n");
    ok ( GetLastError() == 0xdeadbeef, "Expected 0xdeadbeef, got %d\n",
     GetLastError());

    /* Dummy provider will be deleted, but the function still fails because
     * pwszIsFunctionName and pwszIsFunctionNameFmt2 are not present in the
     * registry.
     */
    SetLastError(0xdeadbeef);
    ret = CryptSIPRemoveProvider(&actionid);
    ok (!ret, "Expected CryptSIPRemoveProvider to fail.\n");
    ok (GetLastError() == ERROR_FILE_NOT_FOUND,
        "Expected ERROR_FILE_NOT_FOUND, got %d.\n", GetLastError());

    /* Everything OK */
    memset(&newprov, 0, sizeof(SIP_ADD_NEWPROVIDER));
    newprov.cbStruct = sizeof(SIP_ADD_NEWPROVIDER);
    newprov.pgSubject = &actionid;
    newprov.pwszDLLFileName = dummydll;
    newprov.pwszGetFuncName = dummyfunction;
    newprov.pwszPutFuncName = dummyfunction;
    newprov.pwszCreateFuncName = dummyfunction;
    newprov.pwszVerifyFuncName = dummyfunction;
    newprov.pwszRemoveFuncName = dummyfunction;
    newprov.pwszIsFunctionNameFmt2 = dummyfunction;
    newprov.pwszIsFunctionName = dummyfunction;
    SetLastError(0xdeadbeef);
    ret = CryptSIPAddProvider(&newprov);
    ok ( ret, "CryptSIPAddProvider should have succeeded\n");
    ok ( GetLastError() == 0xdeadbeef, "Expected 0xdeadbeef, got %d\n",
     GetLastError());

    /* Dummy provider should be deleted */
    SetLastError(0xdeadbeef);
    ret = CryptSIPRemoveProvider(&actionid);
    ok ( ret, "CryptSIPRemoveProvider should have succeeded\n");
    ok ( GetLastError() == 0xdeadbeef, "Expected 0xdeadbeef, got %d\n",
     GetLastError());
}

static void test_SIPRetrieveSubjectGUID(void)
{
    BOOL ret;
    GUID subject;
    HANDLE file;
    static const CHAR windir[] = "windir";
    static const CHAR regeditExe[] = "regedit.exe";
    static const GUID nullSubject  = { 0x0, 0x0, 0x0, { 0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0 }};
    static const WCHAR deadbeef[]  = { 'c',':','\\','d','e','a','d','b','e','e','f','.','d','b','f',0 };
    /* Couldn't find a name for this GUID, it's the one used for 95% of the files */
    static const GUID unknownGUID = { 0xC689AAB8, 0x8E78, 0x11D0, { 0x8C,0x47,0x00,0xC0,0x4F,0xC2,0x95,0xEE }};
    static CHAR  regeditPath[MAX_PATH];
    static WCHAR regeditPathW[MAX_PATH];
    static CHAR path[MAX_PATH];
    static CHAR tempfile[MAX_PATH];
    static WCHAR tempfileW[MAX_PATH];
    DWORD written;

    /* NULL check */
    SetLastError(0xdeadbeef);
    ret = CryptSIPRetrieveSubjectGuid(NULL, NULL, NULL);
    ok ( !ret, "Expected CryptSIPRetrieveSubjectGuid to fail\n");
    ok (GetLastError() == ERROR_INVALID_PARAMETER,
        "Expected ERROR_INVALID_PARAMETER, got %d.\n", GetLastError());

    /* Test with a nonexistent file (hopefully) */
    SetLastError(0xdeadbeef);
    /* Set subject to something other than zeros */
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(deadbeef, NULL, &subject);
    ok ( !ret, "Expected CryptSIPRetrieveSubjectGuid to fail\n");
    ok (GetLastError() == ERROR_FILE_NOT_FOUND,
        "Expected ERROR_FILE_NOT_FOUND, got %d.\n", GetLastError());
    ok ( !memcmp(&subject, &nullSubject, sizeof(GUID)),
        "Expected a NULL GUID for c:\\deadbeef.dbf, not %s\n", show_guid(&subject));

    /* Now with an executable that should exist
     *
     * Use A-functions where possible as that should be available on all platforms
     */
    GetEnvironmentVariableA(windir, regeditPath, MAX_PATH);
    sprintf(regeditPath, "%s\\%s", regeditPath, regeditExe);
    MultiByteToWideChar( CP_ACP, 0, regeditPath,
                         strlen(regeditPath)+1, regeditPathW,
                         sizeof(regeditPathW)/sizeof(regeditPathW[0]) );

    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(regeditPathW, NULL, &subject);
    ok ( ret, "Expected CryptSIPRetrieveSubjectGuid to succeed\n");
    ok ( GetLastError() == ERROR_SUCCESS,
        "Expected ERROR_SUCCESS, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &unknownGUID, sizeof(GUID)),
        "Expected (%s), got (%s).\n", show_guid(&unknownGUID), show_guid(&subject));

    /* The same thing but now with a handle instead of a filename */
    file = CreateFileA(regeditPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(NULL, file, &subject);
    ok ( ret, "Expected CryptSIPRetrieveSubjectGuid to succeed\n");
    ok ( GetLastError() == ERROR_SUCCESS,
        "Expected ERROR_SUCCESS, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &unknownGUID, sizeof(GUID)),
        "Expected (%s), got (%s).\n", show_guid(&unknownGUID), show_guid(&subject));
    CloseHandle(file);

    /* And both */
    file = CreateFileA(regeditPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(regeditPathW, file, &subject);
    ok ( ret, "Expected CryptSIPRetrieveSubjectGuid to succeed\n");
    ok ( GetLastError() == ERROR_SUCCESS,
        "Expected ERROR_SUCCESS, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &unknownGUID, sizeof(GUID)),
        "Expected (%s), got (%s).\n", show_guid(&unknownGUID), show_guid(&subject));
    CloseHandle(file);

    /* Now with an empty file */
    GetTempPathA(sizeof(path), path);
    GetTempFileNameA(path, "sip", 0 , tempfile);
    MultiByteToWideChar( CP_ACP, 0, tempfile,
                         strlen(tempfile)+1, tempfileW,
                         sizeof(tempfileW)/sizeof(tempfileW[0]) );

    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(tempfileW, NULL, &subject);
    ok ( !ret, "Expected CryptSIPRetrieveSubjectGuid to fail\n");
    ok ( GetLastError() == ERROR_FILE_INVALID ||
         GetLastError() == S_OK /* Win98 */,
        "Expected ERROR_FILE_INVALID or S_OK, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &nullSubject, sizeof(GUID)),
        "Expected a NULL GUID for empty file %s, not %s\n", tempfile, show_guid(&subject));

    /* Use a file with a size of 3 (at least < 4) */
    file = CreateFileA(tempfile, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    WriteFile(file, "123", 3, &written, NULL);
    CloseHandle(file);

    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(tempfileW, NULL, &subject);
    ok ( !ret, "Expected CryptSIPRetrieveSubjectGuid to fail\n");
    ok ( GetLastError() == ERROR_INVALID_PARAMETER ||
         GetLastError() == S_OK /* Win98 */,
        "Expected ERROR_INVALID_PARAMETER or S_OK, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &nullSubject, sizeof(GUID)),
        "Expected a NULL GUID for empty file %s, not %s\n", tempfile, show_guid(&subject));

    /* And now >= 4 */
    file = CreateFileA(tempfile, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    WriteFile(file, "1234", 4, &written, NULL);
    CloseHandle(file);

    SetLastError(0xdeadbeef);
    memset(&subject, 1, sizeof(GUID));
    ret = CryptSIPRetrieveSubjectGuid(tempfileW, NULL, &subject);
    ok ( !ret, "Expected CryptSIPRetrieveSubjectGuid to fail\n");
    ok ( GetLastError() == TRUST_E_SUBJECT_FORM_UNKNOWN ||
         GetLastError() == S_OK /* Win98 */,
        "Expected TRUST_E_SUBJECT_FORM_UNKNOWN or S_OK, got 0x%08x\n", GetLastError());
    ok ( !memcmp(&subject, &nullSubject, sizeof(GUID)),
        "Expected a NULL GUID for empty file %s, not %s\n", tempfile, show_guid(&subject));

    /* Clean up */
    DeleteFileA(tempfile);
}

static void test_SIPLoad(void)
{
    BOOL ret;
    GUID subject;
    static GUID dummySubject = { 0xdeadbeef, 0xdead, 0xbeef, { 0xde,0xad,0xbe,0xef,0xde,0xad,0xbe,0xef }};
    static GUID unknown      = { 0xC689AABA, 0x8E78, 0x11D0, { 0x8C,0x47,0x00,0xC0,0x4F,0xC2,0x95,0xEE }};
    SIP_DISPATCH_INFO sdi;
    HMODULE hCrypt;

    /* All NULL */
    SetLastError(0xdeadbeef);
    ret = CryptSIPLoad(NULL, 0, NULL);
    ok ( !ret, "Expected CryptSIPLoad to fail\n");
    todo_wine
        ok ( GetLastError() == ERROR_INVALID_PARAMETER,
            "Expected ERROR_INVALID_PARAMETER, got 0x%08x\n", GetLastError());

    /* Only pSipDispatch NULL */
    SetLastError(0xdeadbeef);
    ret = CryptSIPLoad(&subject, 0, NULL);
    ok ( !ret, "Expected CryptSIPLoad to fail\n");
    todo_wine
        ok ( GetLastError() == ERROR_INVALID_PARAMETER,
            "Expected ERROR_INVALID_PARAMETER, got 0x%08x\n", GetLastError());

    /* No NULLs, but nonexistent pgSubject */
    SetLastError(0xdeadbeef);
    memset(&sdi, 0, sizeof(SIP_DISPATCH_INFO));
    sdi.cbSize = sizeof(SIP_DISPATCH_INFO);
    sdi.pfGet = (pCryptSIPGetSignedDataMsg)0xdeadbeef;
    ret = CryptSIPLoad(&dummySubject, 0, &sdi);
    ok ( !ret, "Expected CryptSIPLoad to fail\n");
    todo_wine
        ok ( GetLastError() == TRUST_E_SUBJECT_FORM_UNKNOWN,
            "Expected TRUST_E_SUBJECT_FORM_UNKNOWN, got 0x%08x\n", GetLastError());
    ok( sdi.pfGet == (pCryptSIPGetSignedDataMsg)0xdeadbeef, "Expected no change to the function pointer\n");

    /* All OK */
    SetLastError(0xdeadbeef);
    memset(&sdi, 0, sizeof(SIP_DISPATCH_INFO));
    sdi.cbSize = sizeof(SIP_DISPATCH_INFO);
    sdi.pfGet = (pCryptSIPGetSignedDataMsg)0xdeadbeef;
    ret = CryptSIPLoad(&unknown, 0, &sdi);
    todo_wine
    {
        ok ( ret, "Expected CryptSIPLoad to succeed\n");
        /* This error will always be there as native searches for the function DllCanUnloadNow
         * in WINTRUST.DLL (in this case). This function is not available in WINTRUST.DLL.
         * For now there's no need to implement this is Wine.
         */
        ok ( GetLastError() == ERROR_PROC_NOT_FOUND,
            "Expected ERROR_PROC_NOT_FOUND, got 0x%08x\n", GetLastError());
        ok( sdi.pfGet != (pCryptSIPGetSignedDataMsg)0xdeadbeef, "Expected a function pointer to be loaded.\n");
    }

    /* The function addresses returned by CryptSIPLoad are actually the addresses of
     * crypt32's own functions. A function calling these addresses will end up first
     * calling crypt32 functions which in it's turn call the equivalent in the SIP
     * as dictated by the given GUID.
     */
    hCrypt = LoadLibrary("crypt32.dll");
    if (hCrypt)
    {
        funcCryptSIPGetSignedDataMsg = (void*)GetProcAddress(hCrypt, "CryptSIPGetSignedDataMsg");
        funcCryptSIPPutSignedDataMsg = (void*)GetProcAddress(hCrypt, "CryptSIPPutSignedDataMsg");
        funcCryptSIPCreateIndirectData = (void*)GetProcAddress(hCrypt, "CryptSIPCreateIndirectData");
        funcCryptSIPVerifyIndirectData = (void*)GetProcAddress(hCrypt, "CryptSIPVerifyIndirectData");
        funcCryptSIPRemoveSignedDataMsg = (void*)GetProcAddress(hCrypt, "CryptSIPRemoveSignedDataMsg");
        if (funcCryptSIPGetSignedDataMsg && funcCryptSIPPutSignedDataMsg && funcCryptSIPCreateIndirectData &&
            funcCryptSIPVerifyIndirectData && funcCryptSIPRemoveSignedDataMsg)
            todo_wine
                ok (sdi.pfGet == funcCryptSIPGetSignedDataMsg &&
                    sdi.pfPut == funcCryptSIPPutSignedDataMsg &&
                    sdi.pfCreate == funcCryptSIPCreateIndirectData &&
                    sdi.pfVerify == funcCryptSIPVerifyIndirectData &&
                    sdi.pfRemove == funcCryptSIPRemoveSignedDataMsg,
                    "Expected function addresses to be from crypt32\n");
        else
            trace("Couldn't load function pointers\n");
 
        FreeLibrary(hCrypt);
    }

    /* Reserved parameter not 0 */
    SetLastError(0xdeadbeef);
    memset(&sdi, 0, sizeof(SIP_DISPATCH_INFO));
    sdi.cbSize = sizeof(SIP_DISPATCH_INFO);
    sdi.pfGet = (pCryptSIPGetSignedDataMsg)0xdeadbeef;
    ret = CryptSIPLoad(&unknown, 1, &sdi);
    ok ( !ret, "Expected CryptSIPLoad to fail\n");
    todo_wine
        ok ( GetLastError() == ERROR_INVALID_PARAMETER,
            "Expected ERROR_INVALID_PARAMETER, got 0x%08x\n", GetLastError());
    ok( sdi.pfGet == (pCryptSIPGetSignedDataMsg)0xdeadbeef, "Expected no change to the function pointer\n");
}

START_TEST(sip)
{
    test_AddRemoveProvider();
    test_SIPRetrieveSubjectGUID();
    test_SIPLoad();
}
