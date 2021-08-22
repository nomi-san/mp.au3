#include <windows.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#define MAX_SUBPOCS 10
#define EXPORT EXTERN_C __declspec(dllexport)

static DWORD g_mainPId = 0;
static HWND g_mainHwnd;

static HANDLE g_subProcs[MAX_SUBPOCS];
static HWND g_subHwnds[MAX_SUBPOCS];
static int g_subCount = 0;

static int g_currentIdx;

#define MESSAGE_CLASS   L"MP_MESSAGE"
#define WM_FROM_MAIN    (WM_USER + 1)
#define WM_FROM_SUB     (WM_USER + 2)

static void (CALLBACK *g_MainListener)(int) = nullptr;
static void (CALLBACK *g_SubListener)(int, int) = nullptr;

EXPORT BOOL MP_IsMain()
{
    return GetCurrentProcessId() == g_mainPId;
}

static LRESULT CALLBACK MessageWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_FROM_MAIN:
            if (g_MainListener) g_MainListener((int)lp);
            break;
        case WM_FROM_SUB:
            if (g_SubListener) g_SubListener((int)wp, (int)lp);
            break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LONG (NTAPI *g_NtResumeProcess)(HANDLE ProcessHandle);
static LONG (NTAPI *g_NtSuspendProcess)(HANDLE ProcessHandle);

EXPORT INT MP_Init()
{
    WNDCLASSW wc{};
    wc.lpfnWndProc = &MessageWndProc;
    wc.lpszClassName = MESSAGE_CLASS;
    RegisterClassW(&wc);

    HWND win = CreateWindowExW(0, MESSAGE_CLASS, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    if (g_mainPId == 0) {
        g_mainHwnd = win;
        g_mainPId = GetCurrentProcessId();

        HMODULE ntdll = GetModuleHandleA("ntdll");
        g_NtResumeProcess = (decltype(g_NtResumeProcess))GetProcAddress(ntdll, "NtResumeProcess");
        g_NtSuspendProcess = (decltype(g_NtResumeProcess))GetProcAddress(ntdll, "NtSuspendProcess");
    }
    else {
        HANDLE main = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_mainPId);
        WriteProcessMemory(main, &g_subHwnds[g_currentIdx], &win, sizeof(HWND), NULL);
        CloseHandle(main);
    }

    return 1;
}

EXPORT void MP_Wait(int index)
{
    if (!MP_IsMain()) return;
    if (g_subCount == 0) return;
    if (index < 0 || index >= g_subCount) return;

    WaitForSingleObject(g_subProcs[index], INFINITE);
}

EXPORT void MP_WaitAll()
{
    if (g_subCount == 0) return;

    WaitForMultipleObjects(g_subCount, g_subProcs, TRUE, INFINITE);
}

static void InjectSelf(HANDLE process)
{
    // Get this dll path;
    WCHAR dll_path[1024]{};
    GetModuleFileNameW((HINSTANCE)&__ImageBase, dll_path, 1024);

    // Create path inside process.
    void *path_addr = VirtualAllocEx(process, NULL, sizeof(dll_path), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(process, path_addr, &dll_path, sizeof(dll_path), NULL);

    // Load this dll.
    HANDLE remote = CreateRemoteThread(process, NULL, 0,
        (LPTHREAD_START_ROUTINE)&LoadLibraryW, path_addr, 0, NULL);

    WaitForSingleObject(remote, 4000);
    CloseHandle(remote);
}

static DWORD CALLBACK Thread_Preload(LPVOID param)
{
    // TODO
    return 0;
}

static void Preload(HANDLE process)
{
    WriteProcessMemory(process, &g_currentIdx, &g_subCount, sizeof(g_currentIdx), NULL);
    WriteProcessMemory(process, &g_mainPId, &g_mainPId, sizeof(g_mainPId), NULL);
    WriteProcessMemory(process, &g_mainHwnd, &g_mainHwnd, sizeof(g_mainHwnd), NULL);

    // TODO

    //HANDLE remote = CreateRemoteThread(process, NULL, 0, &Thread_Preload, NULL, 0, NULL);

    //WaitForSingleObject(remote, 4000);
    //CloseHandle(remote);
}

EXPORT INT MP_Fork(BOOL suspended)
{
    if (!MP_IsMain()) return -1;
    if (g_subCount >= MAX_SUBPOCS) return -1;

    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);

    // Get working dir.
    WCHAR cwd[1024]{};
    GetCurrentDirectoryW(1024, cwd);

    // Get command line.
    LPWSTR cmdLine = GetCommandLineW();

    // Clone process, suspended.
    CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, cwd, &si, &pi);
    g_subProcs[g_subCount] = pi.hProcess;

    // Inject this dll.
    InjectSelf(pi.hProcess);

    // Preload data.
    Preload(pi.hProcess);

    if (!suspended) {
        // Resume the main thread.
        ResumeThread(pi.hThread);
    }

    return g_subCount++;
}

// FNV-1a-32
static UINT32 GetHash(LPCWSTR key, SIZE_T length)
{
    UINT32 hash = 2166136261U;

    for (size_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

class Element
{
public:
    void Init()
    {
        m_hash = 0;
        m_data.vt = VT_NULL;
    }

    void Clear()
    {
        VariantClear(&m_data);
    }

    UINT32 GetHash()
    {
        return m_hash;
    }

    void SetHash(UINT32 hash)
    {
        m_hash = hash;
    }

    VARIANT *GetData()
    {
        return &m_data;
    }

    void SetData(VARIANT *data)
    {
        VariantCopy(&m_data, data);
    }

    bool IsNull()
    {
        return m_data.vt == VT_NULL;
    }

private:
    UINT32 m_hash;
    VARIANT m_data{};
};

static struct MetaData {
    int index;
    UINT32 hash;
    VARIANT value;
    UINT length;
} g_meta;

#include <string>

static DWORD WINAPI Thread_Getter(MetaData *param);
static void GetElement(UINT32 hash, VARIANT *value)
{
    g_meta.hash = hash;
    g_meta.index = g_currentIdx;

    HANDLE main = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_mainPId);

    void *addr = VirtualAllocEx(main, NULL, sizeof(MetaData), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(main, addr, &g_meta, sizeof(MetaData), NULL);

    HANDLE remote = CreateRemoteThread(main, NULL, 0,
        (LPTHREAD_START_ROUTINE)&Thread_Getter, addr, 0, NULL);

    WaitForSingleObject(remote, INFINITE);
    CloseHandle(remote);
    CloseHandle(main);

    if (g_meta.value.vt == VT_BSTR) {
        LPWSTR pre = g_meta.value.bstrVal;
        g_meta.value.bstrVal = SysAllocStringLen(pre, g_meta.length);
        VirtualFree(pre, 0, MEM_RELEASE);
    }

    *value = g_meta.value;
}

static DWORD WINAPI Thread_Setter(MetaData *param);
static void SetElement(UINT32 hash, VARIANT *value)
{
    g_meta.hash = hash;
    g_meta.value = *value;

    HANDLE main = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_mainPId);

    if (value->vt == VT_BSTR) {
        g_meta.length = lstrlenW(value->bstrVal);
        size_t bytes = (g_meta.length + 1) * sizeof(WCHAR);
        void *addr = VirtualAllocEx(main, NULL, bytes, MEM_COMMIT, PAGE_READWRITE);
        WriteProcessMemory(main, addr, g_meta.value.bstrVal, bytes, NULL);
        g_meta.value.bstrVal = (LPWSTR)addr;
    }

    void *addr = VirtualAllocEx(main, NULL, sizeof(MetaData), MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(main, addr, &g_meta, sizeof(MetaData), NULL);

    HANDLE remote = CreateRemoteThread(main, NULL, 0,
        (LPTHREAD_START_ROUTINE)&Thread_Setter, addr, 0, NULL);

    WaitForSingleObject(remote, INFINITE);
    CloseHandle(remote);
    CloseHandle(main);
}

class SharedData : public IDispatch
{
public:
    SharedData()
    {
        m_count = 0;
        m_cmask = -1;
        m_elements = nullptr;
    }

    ~SharedData()
    {
        if (m_elements != nullptr) {
            for (int i = 0; i <= m_cmask; i++) {
                m_elements[i].Clear();
            }

            delete[] m_elements;
            m_elements = nullptr;
        }
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        // Increase ref count.
        return ++m_refCount;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        // Decrease ref count.
        if (--m_refCount == 0) {
            // Self release.
            delete this;
            return 0;
        }
        return m_refCount;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID &riid, void **ppvObject)
    {
        if (riid == IID_IUnknown || riid == IID_IDispatch) {
            this->AddRef();
            *ppvObject = this;
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo)
    {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
    {
        if (ppTInfo) *ppTInfo = NULL;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetIDsOfNames(const IID &riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
    {
        *rgDispId = (LONG)GetHash(*rgszNames, cNames);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, const IID &riid, LCID lcid, WORD wFlags,
        DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
    {
        UINT32 hash = (UINT32)dispIdMember;

        if ((wFlags & DISPATCH_METHOD) || (wFlags & DISPATCH_PROPERTYGET)) {
            if (MP_IsMain()) {
                Element *el = FindElement((UINT32)dispIdMember);
                if (pVarResult != NULL) VariantCopy(pVarResult, el->GetData());
                return S_OK;
            }
            else {
                if (pVarResult != NULL) GetElement(hash, pVarResult);
                return S_OK;
            }
        }
        else if ((wFlags & DISPATCH_PROPERTYPUT) || (wFlags & DISPATCH_PROPERTYPUTREF)) {
            if (MP_IsMain()) {
                Element *el = FindElement((UINT32)dispIdMember);
                el->SetData(&pDispParams->rgvarg[0]);
                return S_OK;
            }
            else {
                SetElement(hash, &pDispParams->rgvarg[0]);
                return S_OK;
            }
        }

        return DISP_E_MEMBERNOTFOUND;
    }

    Element *FindElement(UINT32 hash)
    {
        if (m_count >= (m_cmask + 1) * 0.75) {
            int capMask = m_cmask + 1;
            capMask = capMask < 8 ? 8 : capMask * 2;
            Expand(capMask - 1);
        }

        Element *entry = FindEntry(m_elements, m_cmask, hash);

        bool isNewKey = entry->GetHash() == 0;
        if (isNewKey && entry->IsNull()) {
            m_count++;
        }

        return entry;
    }

    static Element *FindEntry(Element *elements, int cmask, UINT32 hash)
    {
        uint32_t index = hash & cmask;
        Element *tombstone = nullptr;

        for (;;) {
            Element *el = &elements[index];

            if (el->GetHash() == 0) {
                if (el->IsNull()) {
                    return tombstone != nullptr ? tombstone : el;
                }
                else {
                    if (tombstone == nullptr) tombstone = el;
                }
            }
            else if (el->GetHash() == hash) {
                return el;
            }

            index = (index + 1) & cmask;
        }
    }

    void Expand(int capMask)
    {
        Element *elements = new Element[capMask + 1];
        for (int i = 0; i <= capMask; i++) {
            elements[i].Init();
        }

        m_count = 0;
        for (int i = 0; i <= m_cmask; i++) {
            Element *el = &m_elements[i];
            UINT32 hash = el->GetHash();

            if (hash == 0) continue;

            Element *dest = FindEntry(elements, capMask, hash);
            dest->SetHash(hash);
            dest->SetData(el->GetData());
            m_count++;
        }

        this->~SharedData();
        m_cmask = capMask;
        m_elements = elements;
    }

private:
    ULONG m_refCount = 0;

    int m_count;
    int m_cmask;
    Element *m_elements;
};

static SharedData *g_sharedData = nullptr;

static DWORD WINAPI Thread_Getter(MetaData *meta)
{
    HANDLE process = g_subProcs[meta->index];
    Element *el = g_sharedData->FindElement(meta->hash);
    VARIANT data = *el->GetData();

    meta->value = data;

    if (data.vt == VT_BSTR) {
        meta->length = lstrlenW(data.bstrVal);
        size_t bytes = (meta->length + 1) * sizeof(WCHAR);
        meta->value.bstrVal = (LPWSTR)VirtualAllocEx(process, NULL, bytes, MEM_COMMIT, PAGE_READWRITE);
        WriteProcessMemory(process, meta->value.bstrVal, data.bstrVal, bytes, NULL);
    }

    WriteProcessMemory(process, &g_meta, meta, sizeof(MetaData), NULL);

    VirtualFree(meta, 0, MEM_RELEASE);
    return 0;
}

static DWORD WINAPI Thread_Setter(MetaData *meta)
{
    if (meta->value.vt == VT_BSTR) {
        LPWSTR pre = meta->value.bstrVal;
        meta->value.bstrVal = SysAllocStringLen(pre, meta->length);
        VirtualFree(pre, 0, MEM_RELEASE);
    }

    Element *el = g_sharedData->FindElement(meta->hash);
    el->SetData(&meta->value);

    VirtualFree(meta, 0, MEM_RELEASE);
    return 0;
}

EXPORT LPDISPATCH MP_SharedData()
{
    if (g_sharedData == nullptr) {
        g_sharedData = new SharedData();
    }

    return g_sharedData;
}

EXPORT void MP_SendToMain(int message)
{
    if (MP_IsMain()) return;

    SendMessageW(g_mainHwnd, WM_FROM_SUB, (WPARAM)g_currentIdx, (LPARAM)message);
}

EXPORT void MP_SendToSub(int index, int message)
{
    if (!MP_IsMain()) return;
    if (index < 0 || index >= MAX_SUBPOCS) return;

    SendMessageW(g_subHwnds[index], WM_FROM_MAIN, NULL, (LPARAM)message);
}

EXPORT void MP_OnMain(decltype(g_MainListener) callback)
{
    if (MP_IsMain()) return;

    g_MainListener = callback;
}

EXPORT void MP_OffMain(decltype(g_MainListener) callback)
{
    if (MP_IsMain()) return;
    if (g_MainListener != callback) return;

    g_MainListener = nullptr;
}

EXPORT void MP_OnSub(decltype(g_SubListener) callback)
{
    if (!MP_IsMain()) return;

    g_SubListener = callback;
}

EXPORT void MP_OffSub(decltype(g_SubListener) callback)
{
    if (!MP_IsMain()) return;
    if (g_SubListener != callback) return;

    g_SubListener = nullptr;
}

EXPORT void MP_Resume(int index)
{
    if (!MP_IsMain()) return;
    if (index < 0 || index >= MAX_SUBPOCS) return;

    g_NtResumeProcess(g_subProcs[index]);
}

EXPORT void MP_Suspend(int index)
{
    if (!MP_IsMain()) return;
    if (index < 0 || index >= MAX_SUBPOCS) return;

    g_NtSuspendProcess(g_subProcs[index]);
}

BOOL APIENTRY DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    switch (reason) {
        case DLL_PROCESS_DETACH:
            if (MP_IsMain()) {
                for (int i = 0; i < g_subCount; i++) {
                    CloseHandle(g_subProcs[i]);
                }
            }
            break;
    }

    return TRUE;
}