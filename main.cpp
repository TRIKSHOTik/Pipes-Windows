#include <windows.h>
#include <iostream>
#include <vector>
#include <string>

void ErrorExit(const char* msg) {
    std::cerr << msg << " Error: " << GetLastError() << std::endl;
    ExitProcess(1);
}

PROCESS_INFORMATION CreateChildProcessWithHandles(const std::string& cmd,
    HANDLE hStdIn_inheritable, HANDLE hStdOut_inheritable)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdIn_inheritable ? hStdIn_inheritable : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdOut_inheritable ? hStdOut_inheritable : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    std::vector<char> cmdLine(cmd.begin(), cmd.end());
    cmdLine.push_back('\0');

    if (!CreateProcessA(NULL, cmdLine.data(),
        NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        std::string serr = "CreateProcess failed for " + cmd;
        ErrorExit(serr.c_str());
    }
    CloseHandle(pi.hThread);
    return pi;
}

int main() {
    HANDLE hStdinRead = NULL, hStdinWrite = NULL;
    HANDLE hMtoA_Read = NULL, hMtoA_Write = NULL;
    HANDLE hAtoP_Read = NULL, hAtoP_Write = NULL;
    HANDLE hPtoS_Read = NULL, hPtoS_Write = NULL;

    if (!CreatePipe(&hStdinRead, &hStdinWrite, NULL, 0)) ErrorExit("CreatePipe stdin failed.");
    if (!CreatePipe(&hMtoA_Read, &hMtoA_Write, NULL, 0)) ErrorExit("CreatePipe M->A failed.");
    if (!CreatePipe(&hAtoP_Read, &hAtoP_Write, NULL, 0)) ErrorExit("CreatePipe A->P failed.");
    if (!CreatePipe(&hPtoS_Read, &hPtoS_Write, NULL, 0)) ErrorExit("CreatePipe P->S failed.");

    HANDLE me = GetCurrentProcess();

    std::vector<HANDLE> procHandles;

    std::cout << "Starting process S...\n";
    HANDLE s_stdin_inheritable = NULL;
    if (!DuplicateHandle(me, hPtoS_Read, me, &s_stdin_inheritable, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for S stdin failed.");
    PROCESS_INFORMATION piS = CreateChildProcessWithHandles("processS.exe", s_stdin_inheritable, NULL);
    procHandles.push_back(piS.hProcess);
    CloseHandle(s_stdin_inheritable);
    CloseHandle(hPtoS_Read);

    std::cout << "Process S started.\n";

    std::cout << "Starting process P...\n";
    HANDLE p_stdin_inher = NULL, p_stdout_inher = NULL;
    if (!DuplicateHandle(me, hAtoP_Read, me, &p_stdin_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for P stdin failed.");
    if (!DuplicateHandle(me, hPtoS_Write, me, &p_stdout_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for P stdout failed.");

    PROCESS_INFORMATION piP = CreateChildProcessWithHandles("processP.exe", p_stdin_inher, p_stdout_inher);
    procHandles.push_back(piP.hProcess);
    CloseHandle(p_stdin_inher);
    CloseHandle(p_stdout_inher);
    CloseHandle(hAtoP_Read);
    CloseHandle(hPtoS_Write);

    std::cout << "Process P started.\n";

    std::cout << "Starting process A...\n";
    HANDLE a_stdin_inher = NULL, a_stdout_inher = NULL;
    if (!DuplicateHandle(me, hMtoA_Read, me, &a_stdin_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for A stdin failed.");
    if (!DuplicateHandle(me, hAtoP_Write, me, &a_stdout_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for A stdout failed.");

    PROCESS_INFORMATION piA = CreateChildProcessWithHandles("processA.exe", a_stdin_inher, a_stdout_inher);
    procHandles.push_back(piA.hProcess);
    CloseHandle(a_stdin_inher);
    CloseHandle(a_stdout_inher);
    CloseHandle(hMtoA_Read);
    CloseHandle(hAtoP_Write);

    std::cout << "Process A started.\n";

    std::cout << "Starting process M...\n";
    HANDLE m_stdin_inher = NULL, m_stdout_inher = NULL;
    if (!DuplicateHandle(me, hStdinRead, me, &m_stdin_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for M stdin failed.");
    if (!DuplicateHandle(me, hMtoA_Write, me, &m_stdout_inher, 0, TRUE, DUPLICATE_SAME_ACCESS))
        ErrorExit("DuplicateHandle for M stdout failed.");

    PROCESS_INFORMATION piM = CreateChildProcessWithHandles("processM.exe", m_stdin_inher, m_stdout_inher);
    procHandles.push_back(piM.hProcess);
    CloseHandle(m_stdin_inher);
    CloseHandle(m_stdout_inher);
    CloseHandle(hStdinRead);
    CloseHandle(hMtoA_Write);

    std::cout << "Process M started.\n";

    std::string input = "1 2 3\n";
    DWORD written = 0;
    if (!WriteFile(hStdinWrite, input.c_str(), (DWORD)input.size(), &written, NULL))
        ErrorExit("WriteFile to stdin pipe failed.");
    std::cout << "Input sent: " << input;
    CloseHandle(hStdinWrite);

    std::cout << "Waiting for completion...\n";
    if (!procHandles.empty()) {
        if (WaitForMultipleObjects((DWORD)procHandles.size(), procHandles.data(), TRUE, INFINITE) == WAIT_FAILED)
            ErrorExit("WaitForMultipleObjects failed.");
        std::cout << "All processes completed.\n";
    }

    for (HANDLE h : procHandles) CloseHandle(h);
    system("pause");
    return 0;
}
