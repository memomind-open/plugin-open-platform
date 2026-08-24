param(
    [switch]$Smoke,
    [Alias("test-combat")]
    [switch]$TestCombat,
    [Alias("test-lifecycle")]
    [switch]$TestLifecycle,
    [switch]$BuildOnly
)

$ErrorActionPreference = "Stop"
$sdkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outputDir = Join-Path $sdkRoot "build-host\simulator"
$executableName = if ($TestCombat -or $TestLifecycle) { "gm-fighter-arena-autotest.exe" } else { "gm-fighter-arena-simulator.exe" }
$executable = Join-Path $outputDir $executableName
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Installer was not found. Install Visual Studio Build Tools with Desktop development with C++."
}

$vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) {
    throw "Visual Studio C++ Build Tools were not found."
}
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$dependencies = @(
    (Join-Path $PSScriptRoot "win32_host.c"),
    (Join-Path $PSScriptRoot "fighter_arena_adapter.c"),
    (Join-Path $PSScriptRoot "simulator.h"),
    (Join-Path $sdkRoot "include\gm_plugin.h"),
    (Join-Path $sdkRoot "include\gm_plugin_protocol.h"),
    (Join-Path $sdkRoot "examples\game\fighter_arena\fighter_arena.c"),
    (Join-Path $sdkRoot "examples\game\fighter_arena\zen_combat_sprites.h"),
    (Join-Path $sdkRoot "examples\game\fighter_arena\rival_combat_sprites.h"),
    (Join-Path $sdkRoot "examples\game\fighter_arena\zen_hurt_sprites.h"),
    (Join-Path $sdkRoot "examples\game\fighter_arena\rival_hurt_sprites.h")
)
$needsBuild = -not (Test-Path $executable)
if (-not $needsBuild) {
    $builtAt = (Get-Item $executable).LastWriteTimeUtc
    $needsBuild = $dependencies.Where({ (Get-Item $_).LastWriteTimeUtc -gt $builtAt }).Count -gt 0
}

if ($needsBuild) {
    $responseFile = Join-Path $outputDir "simulator.rsp"
    $arguments = @(
        "/nologo", "/std:c11", "/W4", "/WX", "/O2", "/MT",
        "/D_CRT_SECURE_NO_WARNINGS",
        "/I`"$($sdkRoot)\include`"", "/I`"$PSScriptRoot`"",
        "`"$($dependencies[0])`"", "`"$($dependencies[1])`"",
        "/Fe`"$executable`"",
        "/link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib"
    )
    Set-Content -Path $responseFile -Value ($arguments -join "`r`n") -Encoding Ascii
    $command = "cd /d `"$outputDir`" && call `"$vcvars`" >nul && cl @`"$responseFile`""
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Simulator build failed with exit code $LASTEXITCODE."
    }
    Write-Host "Built: $executable"
} else {
    Write-Host "Up to date: $executable"
}

if ($BuildOnly) { exit 0 }
Push-Location $outputDir
try {
    if ($Smoke) {
        $testProcess = Start-Process -FilePath $executable -ArgumentList "--smoke" -Wait -PassThru -WindowStyle Hidden
        $simulatorExitCode = $testProcess.ExitCode
        if ($simulatorExitCode -eq 0) {
            Write-Host "Smoke test passed."
        }
    } elseif ($TestCombat) {
        $testProcess = Start-Process -FilePath $executable -ArgumentList "--test-combat" -Wait -PassThru -WindowStyle Hidden
        $simulatorExitCode = $testProcess.ExitCode
        $report = Join-Path $outputDir "test-results\report.txt"
        if (Test-Path $report) {
            Get-Content $report
        }
    } elseif ($TestLifecycle) {
        $testProcess = Start-Process -FilePath $executable -ArgumentList "--test-lifecycle" -Wait -PassThru -WindowStyle Hidden
        $simulatorExitCode = $testProcess.ExitCode
        $report = Join-Path $outputDir "lifecycle-results\report.txt"
        if (Test-Path $report) {
            Get-Content $report
        }
    } else {
        & $executable
        $simulatorExitCode = $LASTEXITCODE
    }
} finally {
    Pop-Location
}
exit $simulatorExitCode
