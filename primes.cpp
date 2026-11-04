
#include <iostream>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

using namespace std;

const int PROCESS_COUNT = 10;

// Verifica daca un numar este prim
bool isPrime(int n) {
    if (n < 2)
        return false;

    for (int i = 2; i <= n / i; i++) {
        if (n % i == 0)
            return false;
    }

    return true;
}

// Codul executat de fiecare proces copil
int worker() {
    int start, end;

    cin >> start >> end;

    for (int i = start; i <= end; i++) {
        if (isPrime(i))
            cout << i << " ";
    }

    cout << endl;
    cout.flush();

    return 0;
}

#ifdef _WIN32

// Trimite date prin pipe in Windows
bool writePipe(HANDLE pipe, const string& data) {
    DWORD written;
    return WriteFile(pipe, data.c_str(),
                     (DWORD)data.size(), &written, NULL)
           && written == data.size();
}

// Implementarea pentru Windows
int runWindows(char* argv[]) {
    char exePath[MAX_PATH];

    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    for (int i = 0; i < PROCESS_COUNT; i++) {

        HANDLE childInput, parentInput;
        HANDLE parentOutput, childOutput;

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        // Cream cele doua pipe-uri
        if (!CreatePipe(&childInput, &parentInput, &sa, 0) ||
            !CreatePipe(&parentOutput, &childOutput, &sa, 0)) {
            cerr << "Eroare la crearea pipe-urilor!\n";
            return 1;
        }

        // Partea parintelui nu trebuie mostenita
        SetHandleInformation(parentInput,
                             HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(parentOutput,
                             HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = childInput;
        si.hStdOutput = childOutput;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

        PROCESS_INFORMATION pi = {};

        string command = "\"" + string(exePath) + "\" --worker";

        // Cream procesul copil
        BOOL created = CreateProcessA(
            exePath,
            command.data(),
            NULL, NULL, TRUE, 0,
            NULL, NULL, &si, &pi
        );

        CloseHandle(childInput);
        CloseHandle(childOutput);

        if (!created) {
            cerr << "Eroare la crearea procesului!\n";
            CloseHandle(parentInput);
            CloseHandle(parentOutput);
            return 1;
        }

        int start = i * 1000 + 1;
        int end = (i + 1) * 1000;

        string message = to_string(start) + " "
                       + to_string(end) + "\n";

        // Trimitem intervalul copilului
        writePipe(parentInput, message);
        CloseHandle(parentInput);

        cout << "Procesul " << i + 1 << " ("
             << start << "-" << end << "): ";

        // Citim numerele prime trimise de copil
        char buffer[4096];
        DWORD bytesRead;

        while (ReadFile(parentOutput, buffer,
                        sizeof(buffer), &bytesRead, NULL)
               && bytesRead > 0) {
            cout.write(buffer, bytesRead);
        }

        cout.flush();

        WaitForSingleObject(pi.hProcess, INFINITE);

        CloseHandle(parentOutput);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return 0;
}

#else

// Trimite date prin pipe in Linux
void writePipe(int fd, const string& data) {
    size_t total = 0;

    while (total < data.size()) {
        ssize_t written = write(
            fd, data.data() + total, data.size() - total
        );

        if (written <= 0)
            break;

        total += written;
    }
}

// Implementarea pentru Linux
int runLinux() {

    for (int i = 0; i < PROCESS_COUNT; i++) {

        int parentToChild[2];
        int childToParent[2];

        // Cream doua pipe-uri
        if (pipe(parentToChild) == -1 ||
            pipe(childToParent) == -1) {
            cerr << "Eroare la crearea pipe-urilor!\n";
            return 1;
        }

        // Cream procesul copil
        pid_t pid = fork();

        if (pid == -1) {
            cerr << "Eroare la fork!\n";
            return 1;
        }

        if (pid == 0) {
            // CODUL COPILULUI

            close(parentToChild[1]);
            close(childToParent[0]);

            dup2(parentToChild[0], STDIN_FILENO);
            dup2(childToParent[1], STDOUT_FILENO);

            close(parentToChild[0]);
            close(childToParent[1]);

            worker();

            _exit(0);
        }
        else {
            // CODUL PARINTELUI

            close(parentToChild[0]);
            close(childToParent[1]);

            int start = i * 1000 + 1;
            int end = (i + 1) * 1000;

            string message = to_string(start) + " "
                           + to_string(end) + "\n";

            // Trimitem intervalul copilului
            writePipe(parentToChild[1], message);
            close(parentToChild[1]);

            cout << "Procesul " << i + 1 << " ("
                 << start << "-" << end << "): ";

            // Citim numerele prime trimise de copil
            char buffer[4096];
            ssize_t bytesRead;

            while ((bytesRead = read(
                childToParent[0], buffer, sizeof(buffer)
            )) > 0) {
                cout.write(buffer, bytesRead);
            }

            cout.flush();

            close(childToParent[0]);

            waitpid(pid, NULL, 0);
        }
    }

    return 0;
}

#endif

int main(int argc, char* argv[]) {

    // Daca programul este pornit ca proces copil
    if (argc > 1 && string(argv[1]) == "--worker")
        return worker();

#ifdef _WIN32
    return runWindows(argv);
#else
    return runLinux();
#endif
}
