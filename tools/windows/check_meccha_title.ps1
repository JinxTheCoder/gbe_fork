$ErrorActionPreference = 'Stop'
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('MECCHA dialog runtime report')
$lines.Add([DateTime]::Now.ToString('yyyy/MM/dd HH:mm:ss'))
$owners = [System.Collections.Generic.HashSet[int]]::new()

try {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
namespace MecchaTitleCheck {
    public class WindowInfo {
        public uint ProcessId;
        public string ClassName;
        public string Caption;
        public bool NativeCaption;
        public bool Unicode;
    }
    public static class Native {
        private delegate bool EnumProc(IntPtr hwnd, IntPtr param);
        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumProc callback, IntPtr param);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetClassNameW(IntPtr hwnd, StringBuilder text, int count);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowTextW(IntPtr hwnd, StringBuilder text, int count);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
        [DllImport("user32.dll")] private static extern bool IsWindowUnicode(IntPtr hwnd);
        [DllImport("user32.dll", EntryPoint="GetWindowLongPtrW")] private static extern IntPtr GetWindowLongPtrW(IntPtr hwnd, int index);
        [DllImport("user32.dll", EntryPoint="GetWindowLongW")] private static extern int GetWindowLongW(IntPtr hwnd, int index);
        public static WindowInfo[] Collect() {
            var result = new List<WindowInfo>();
            EnumWindows(delegate(IntPtr hwnd, IntPtr param) {
                var name = new StringBuilder(256);
                GetClassNameW(hwnd, name, name.Capacity);
                if (name.ToString().IndexOf("MECCHA", StringComparison.OrdinalIgnoreCase) < 0) return true;
                var caption = new StringBuilder(1024);
                GetWindowTextW(hwnd, caption, caption.Capacity);
                uint pid;
                GetWindowThreadProcessId(hwnd, out pid);
                long style = IntPtr.Size == 8 ? GetWindowLongPtrW(hwnd, ~15).ToInt64() : GetWindowLongW(hwnd, ~15);
                result.Add(new WindowInfo {
                    ProcessId=pid, ClassName=name.ToString(), Caption=caption.ToString(),
                    NativeCaption=(style & 0x00C00000) == 0x00C00000,
                    Unicode=IsWindowUnicode(hwnd)
                });
                return true;
            }, IntPtr.Zero);
            return result.ToArray();
        }
    }
}
'@
    $windows = [MecchaTitleCheck.Native]::Collect()
    if ($windows.Count -eq 0) {
        $lines.Add('No MECCHA dialog found. Keep the affected dialog open while running this check.')
        foreach ($p in [Diagnostics.Process]::GetProcesses()) {
            if ($p.ProcessName.ToLowerInvariant().Contains('penguinhotel')) {
                $null = $owners.Add($p.Id)
            }
        }
    }
    foreach ($window in $windows) {
        $lines.Add('')
        $lines.Add('Window class: ' + $window.ClassName)
        $lines.Add('Window title: [' + $window.Caption + ']')
        $lines.Add('Native caption: ' + $window.NativeCaption)
        $lines.Add('Unicode window: ' + $window.Unicode)
        $lines.Add('Owner process ID: ' + $window.ProcessId)
        $null = $owners.Add([int]$window.ProcessId)
    }
    foreach ($ownerId in $owners) {
        $lines.Add('')
        try {
            $process = [Diagnostics.Process]::GetProcessById($ownerId)
            $lines.Add('Game executable: ' + $process.MainModule.FileName)
            foreach ($module in $process.Modules) {
                if (!$module.ModuleName.ToLowerInvariant().Contains('steam')) { continue }
                $lines.Add('Loaded module: ' + $module.FileName)
                try {
                    $bytes = [IO.File]::ReadAllBytes($module.FileName)
                    $hash = [Security.Cryptography.SHA256]::Create()
                    $digest = [BitConverter]::ToString($hash.ComputeHash($bytes)).Replace([string][char]45, '')
                    $hash.Dispose()
                    $lines.Add('SHA256: ' + $digest)
                    $text = [Text.Encoding]::ASCII.GetString($bytes)
                    $markers = @([regex]::Matches($text, 'GBE_MECCHA_WORKSHOP_(?:MESSAGE|IMPORT|GIF)_V\d+') | ForEach-Object { $_.Value } | Sort-Object -Unique)
                    $lines.Add('DLL window versions: ' + [string]::Join(', ', [string[]]$markers))
                } catch {
                    $lines.Add('Module inspection error: ' + $_.Exception.Message)
                }
            }
        } catch {
            $lines.Add('Process inspection error: ' + $_.Exception.Message)
        }
    }
} catch {
    $lines.Add('Diagnostic error: ' + $_.Exception.Message)
}

$report = $env:MECCHA_TITLE_REPORT_PATH
if (!$report) {
    $report = [IO.Path]::Combine([Environment]::GetFolderPath('Desktop'), 'MECCHA_Title_Report.txt')
}
$report = [IO.Path]::GetFullPath($report)
try {
    [IO.File]::WriteAllLines($report, $lines, [Text.UTF8Encoding]::new($false))
} catch {
    $report = [IO.Path]::Combine([IO.Path]::GetTempPath(), 'MECCHA_Title_Report.txt')
    [IO.File]::WriteAllLines($report, $lines, [Text.UTF8Encoding]::new($false))
}
Write-Host ('Report saved: ' + $report)
if ($env:MECCHA_TITLE_NO_VIEWER -ne '1') {
    Start-Process -FilePath 'notepad.exe' -ArgumentList ('"' + $report + '"')
}
