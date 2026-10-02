$ErrorActionPreference = 'Stop'
$expectedNew = '0ACE29E50E287E1694CFE890191D932CE2096E783059608254BBAAA9F6FCBA4C'
$expectedOld = '88C1D9D7F964F10EE6CAE9802433D0C9292D9596E391F1120CC7BAE599E5761F'
$target = 'C:\MECCHA CHAMELEON\Engine\Binaries\ThirdParty\Steamworks\Steamv157\Win64\steam_api64.dll'
$staged = $null
$backup = $null
$replaced = $false

function GetDllHash([string]$path) {
    $stream = [IO.File]::OpenRead($path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace([string][char]45, '')
    } finally {
        $stream.Dispose()
        $sha.Dispose()
    }
}

function RequireGameClosed {
    foreach ($p in [Diagnostics.Process]::GetProcesses()) {
        if ($p.ProcessName.StartsWith('PenguinHotel', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Close MECCHA CHAMELEON completely, then run this installer again.'
        }
    }
}

try {
    $package = $env:MECCHA_TITLE_PACKAGE_DIR
    if (!$package) { $package = $PSScriptRoot }
    $source = [IO.Path]::Combine($package, 'steam_api64.dll')
    if (![IO.File]::Exists($source)) {
        throw 'Extract the entire ZIP into a folder before running Install_MECCHA_Title.cmd.'
    }
    if ((GetDllHash $source) -ne $expectedNew) {
        throw 'The packaged DLL failed verification. The installed DLL was not changed.'
    }
    if ($env:MECCHA_TITLE_INSTALL_TEST_ROOT) {
        $testRoot = [IO.Path]::GetFullPath($env:MECCHA_TITLE_INSTALL_TEST_ROOT)
        $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char]92) + [char]92
        if (!$testRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'The test destination must be inside the temporary directory.'
        }
        if ($env:MECCHA_TITLE_INSTALL_TEST_ORIGINAL_HASH -notmatch '^[A-Fa-f0-9]{64}$') {
            throw 'The test original hash is missing or invalid.'
        }
        $target = [IO.Path]::Combine($testRoot, 'steam_api64.dll')
        $expectedOld = $env:MECCHA_TITLE_INSTALL_TEST_ORIGINAL_HASH
    }
    RequireGameClosed
    if (![IO.File]::Exists($target)) {
        throw ('The DLL path from the runtime report was not found: ' + $target)
    }
    $actualOld = GetDllHash $target
    if ($actualOld -eq $expectedNew) {
        Write-Host 'V15 is already installed and verified. Relaunch MECCHA CHAMELEON.'
        exit 0
    }
    if ($actualOld -ne $expectedOld) {
        throw ('The installed DLL changed since the report. Installation stopped. Current SHA256: ' + $actualOld)
    }
    $token = [Guid]::NewGuid().ToString('N')
    $staged = $target + '.TitleRepair.' + $token + '.tmp'
    $backup = $target + '.BeforeTitleRepair.' + $token + '.bak'
    [IO.File]::Copy($source, $staged, $false)
    if ((GetDllHash $staged) -ne $expectedNew) { throw 'The staged DLL failed verification.' }
    RequireGameClosed
    [IO.File]::Replace($staged, $target, $backup)
    $replaced = $true
    if ((GetDllHash $target) -ne $expectedNew -or (GetDllHash $backup) -ne $expectedOld) {
        throw 'Installed DLL or backup verification failed.'
    }
    Write-Host 'Installed and verified the V15 title build.'
    Write-Host ('DLL: ' + $target)
    Write-Host ('Original backup: ' + $backup)
    Write-Host 'Relaunch MECCHA CHAMELEON to load the new title code.'
    exit 0
} catch {
    if ($replaced -and $backup -and [IO.File]::Exists($backup)) {
        try {
            [IO.File]::Copy($backup, $target, $true)
            Write-Host 'The original DLL was restored.'
        } catch {
            Write-Host ('Restore failed. The original DLL is preserved at: ' + $backup)
        }
    }
    Write-Host ('Installation stopped: ' + $_.Exception.Message)
    exit 1
} finally {
    if ($staged -and [IO.File]::Exists($staged)) { [IO.File]::Delete($staged) }
}
