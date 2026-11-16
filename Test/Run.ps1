param(
    [ValidateSet('Win32', 'x64')]
    [string]$Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot)
try {
    $env:_CL_ = '/source-charset:.932'
    & msbuild BonCasClient.sln /m /p:Configuration=Release "/p:Platform=$Platform" /p:PlatformToolset=v143 /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw 'DLL build failed' }
    New-Item -ItemType Directory -Force artifacts | Out-Null
    & cl /nologo /EHsc /std:c++14 /W4 /LD Test\LocalReader.cpp /Fo:artifacts\ /Fe:artifacts\LocalReader.dll /link /DEF:Test\LocalReader.def
    if ($LASTEXITCODE -ne 0) { throw 'Fixture build failed' }
    & cl /nologo /EHsc /std:c++14 /W4 /I BonCasClient /I Common /DUNICODE /D_UNICODE Test\SafetyTest.cpp Common\SmartSock.cpp BonCasClient\CasProxy.cpp /Fo:artifacts\ /Fe:artifacts\SafetyTest.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }
    $dll = if ($Platform -eq 'x64') { 'x64\Release\BonCasClient.dll' } else { 'Release\BonCasClient.dll' }
    & .\artifacts\SafetyTest.exe $dll artifacts\LocalReader.dll
    if ($LASTEXITCODE -ne 0) { throw 'Safety tests failed' }
    & cl /nologo /EHsc /std:c++14 /W4 Test\LocalProviderTest.cpp /Fo:artifacts\ /Fe:artifacts\LocalProviderTest.exe
    if ($LASTEXITCODE -ne 0) { throw 'Local provider test build failed' }
    $artifactRoot = [IO.Path]::GetFullPath((Join-Path (Get-Location).Path 'artifacts'))
    $scratch = Join-Path $artifactRoot ('LocalProvider-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory $scratch | Out-Null
    $previousScratch = $env:BONCAS_TEST_SCRATCH
    $env:BONCAS_TEST_SCRATCH = $scratch
    try {
        foreach ($scenario in @('success', 'disconnect', 'disabled', 'missing', 'relative', 'self', 'invalid', 'establish-failed', 'empty', 'list-failed', 'cycle')) {
            & .\artifacts\LocalProviderTest.exe $dll artifacts\LocalReader.dll $scenario
            if ($LASTEXITCODE -ne 0) { throw "Local provider test failed: $scenario" }
        }
    } finally {
        $env:BONCAS_TEST_SCRATCH = $previousScratch
        # The child has exited, so even pinned providers are now unmapped.
        $resolvedScratch = [IO.Path]::GetFullPath($scratch)
        if (-not $resolvedScratch.StartsWith($artifactRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Scratch directory is outside artifacts'
        }
        Remove-Item -LiteralPath $resolvedScratch -Recurse -Force
    }
} finally {
    Pop-Location
}
