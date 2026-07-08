param(
    [string]$CMakePath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
    [string]$SupportDir = "C:\Users\seung\OneDrive\Desktop\Fission-CE-KR-release",
    [string]$OutputRoot = "D:\Translation",
    [string]$PackageName = "Fission-CE-KR-windows-x64-latest",
    [string]$PythonCommand = "python",
    [string]$KoreanArtDir = "D:\Translation\Fission-CE-KR-art-intrface",
    [switch]$NoKoreanArt,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$repoRoot = $repoRoot.Path

if (-not (Test-Path -LiteralPath $CMakePath)) {
    $cmakeCommand = "cmake"
} else {
    $cmakeCommand = $CMakePath
}

$supportRoot = Resolve-Path -LiteralPath $SupportDir
$outputRootPath = Resolve-Path -LiteralPath $OutputRoot
$stageDir = Join-Path $outputRootPath $PackageName
$zipPath = Join-Path $outputRootPath "$PackageName.zip"

$requiredSupportFiles = @(
    "fission.cfg",
    "FissionConfigEditor.exe",
    "data\fonts\korean"
)

foreach ($relativePath in $requiredSupportFiles) {
    $candidate = Join-Path $supportRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Missing support file or directory: $candidate"
    }
}

Push-Location $repoRoot
try {
    if (-not $SkipBuild) {
        & $cmakeCommand --preset windows-x64
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed with exit code $LASTEXITCODE"
        }

        & $cmakeCommand --build --preset windows-x64-release --target fallout2-ce
        if ($LASTEXITCODE -ne 0) {
            throw "CMake build failed with exit code $LASTEXITCODE"
        }
    }

    $exePath = Join-Path $repoRoot "out\build\windows-x64\RelWithDebInfo\fallout-fission-x64.exe"
    $datPath = Join-Path $repoRoot "os\macos\fission.dat"

    if (-not (Test-Path -LiteralPath $exePath)) {
        throw "Built executable not found: $exePath"
    }
    if (-not (Test-Path -LiteralPath $datPath)) {
        throw "fission.dat not found: $datPath"
    }

    if (-not $NoKoreanArt) {
        $converterPath = Join-Path $repoRoot "tools\convert_localization_bmp_to_frm.py"
        if (-not (Test-Path -LiteralPath $converterPath)) {
            throw "Korean art converter not found: $converterPath"
        }

        & $PythonCommand $converterPath `
            --source (Join-Path $repoRoot "files\localization_ko") `
            --templates (Join-Path $repoRoot "files\fission\art\intrface") `
            --palette (Join-Path $repoRoot "files\localization_ko\Fallout.act") `
            --output $KoreanArtDir
        if ($LASTEXITCODE -ne 0) {
            throw "Korean art conversion failed with exit code $LASTEXITCODE"
        }
    }

    if (Test-Path -LiteralPath $stageDir) {
        Remove-Item -LiteralPath $stageDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $stageDir "data\fonts") | Out-Null

    Copy-Item -LiteralPath $exePath -Destination (Join-Path $stageDir "fallout-fission-x64.exe") -Force
    Copy-Item -LiteralPath $datPath -Destination (Join-Path $stageDir "fission.dat") -Force
    Copy-Item -LiteralPath (Join-Path $supportRoot "fission.cfg") -Destination (Join-Path $stageDir "fission.cfg") -Force
    Copy-Item -LiteralPath (Join-Path $supportRoot "FissionConfigEditor.exe") -Destination (Join-Path $stageDir "FissionConfigEditor.exe") -Force
    Copy-Item -LiteralPath (Join-Path $supportRoot "data\fonts\korean") -Destination (Join-Path $stageDir "data\fonts\korean") -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $repoRoot "KOREAN_COMPATIBILITY.md") -Destination (Join-Path $stageDir "KOREAN_COMPATIBILITY.md") -Force
    Copy-Item -LiteralPath (Join-Path $repoRoot "MULTILINGUAL_TTF_SUPPORT.md") -Destination (Join-Path $stageDir "MULTILINGUAL_TTF_SUPPORT.md") -Force

    if (-not $NoKoreanArt) {
        if (-not (Test-Path -LiteralPath $KoreanArtDir)) {
            throw "Converted Korean art directory not found: $KoreanArtDir"
        }

        $intrfaceDir = Join-Path $stageDir "data\art\intrface"
        New-Item -ItemType Directory -Force -Path $intrfaceDir | Out-Null
        Copy-Item -Path (Join-Path $KoreanArtDir "*") -Destination $intrfaceDir -Force

        $localizedIntrfaceDir = Join-Path $stageDir "data\art\korean\intrface"
        New-Item -ItemType Directory -Force -Path $localizedIntrfaceDir | Out-Null
        Copy-Item -Path (Join-Path $KoreanArtDir "*") -Destination $localizedIntrfaceDir -Force

        $pipFrm = Join-Path $KoreanArtDir "PIP.frm"
        if (Test-Path -LiteralPath $pipFrm) {
            Copy-Item -LiteralPath $pipFrm -Destination (Join-Path $intrfaceDir "PIPBOY.frm") -Force
            Copy-Item -LiteralPath $pipFrm -Destination (Join-Path $localizedIntrfaceDir "PIPBOY.frm") -Force
        }
    }

    Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -Force

    Get-Item -LiteralPath $zipPath | Select-Object Length,FullName
} finally {
    Pop-Location
}
