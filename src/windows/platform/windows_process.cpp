// Mcaster1DAWCast — Windows subprocess helpers
// Copyright (C) 2026 David St. John <davestj@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "windows_process.h"
#include "compat_windows.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace dawcast {

ProcessResult WindowsProcess::run(const QString&     exe,
                                  const QStringList& args,
                                  int                timeoutMs) {
    ProcessResult result;

    QString cmdLine = QLatin1Char('"') + QDir::toNativeSeparators(exe) + QLatin1Char('"');
    for (const QString& a : args) {
        cmdLine += QLatin1Char(' ');
        if (a.contains(QLatin1Char(' ')) || a.contains(QLatin1Char('\t'))) {
            cmdLine += QLatin1Char('"') + a + QLatin1Char('"');
        } else {
            cmdLine += a;
        }
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE outR = nullptr, outW = nullptr, errR = nullptr, errW = nullptr;
    if (!CreatePipe(&outR, &outW, &sa, 0) || !CreatePipe(&errR, &errW, &sa, 0)) {
        return result;
    }
    SetHandleInformation(outR, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = outW;
    si.hStdError  = errW;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    std::wstring wCmd = platform::win::utf8_to_wide(cmdLine.toUtf8().constData());
    BOOL ok = CreateProcessW(nullptr, wCmd.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                             nullptr, nullptr, &si, &pi);
    CloseHandle(outW);
    CloseHandle(errW);

    if (!ok) {
        CloseHandle(outR);
        CloseHandle(errR);
        return result;
    }

    auto drain = [](HANDLE h, QString& into) {
        char buf[4096];
        DWORD read = 0;
        while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0) {
            into.append(QString::fromLocal8Bit(buf, static_cast<int>(read)));
        }
    };
    drain(outR, result.stdoutText);
    drain(errR, result.stderrText);

    DWORD wait = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeoutMs));
    if (wait == WAIT_TIMEOUT) {
        result.timedOut = true;
        TerminateProcess(pi.hProcess, 1);
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    result.exitCode = static_cast<int>(code);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outR);
    CloseHandle(errR);

    return result;
}

QString WindowsProcess::resolveHelper(const QString& name) {
    const QString exeDir  = QCoreApplication::applicationDirPath();
    const QString bundled = QDir(exeDir).filePath(QStringLiteral("tools/") + name + QStringLiteral(".exe"));
    if (QFileInfo::exists(bundled)) return bundled;

    const QString onPath = QStandardPaths::findExecutable(name);
    return onPath;
}

} // namespace dawcast
