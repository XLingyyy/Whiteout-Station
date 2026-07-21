param(
    [string]$MakeHumanRoot = (Join-Path $env:LOCALAPPDATA 'makehuman-community')
)

$ErrorActionPreference = 'Stop'
$workspaceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
$cacheRoot = Join-Path $workspaceRoot '.codex_tmp\makehuman'
$installer = Join-Path $cacheRoot 'app\makehuman-community_1.3.0.exe'
$suitsRoot = Join-Path $cacheRoot 'suits03'
$pluginSource = Join-Path $PSScriptRoot 'MakeHumanPlugin\9_ws_autogen'

& python -X utf8 (Join-Path $PSScriptRoot 'download_makehuman_inputs.py')
if ($LASTEXITCODE -ne 0) {
    throw 'MakeHuman input download failed.'
}

if (-not (Test-Path -LiteralPath (Join-Path $MakeHumanRoot 'makehuman\makehuman.py'))) {
    if (-not (Test-Path -LiteralPath $installer)) {
        throw "Verified MakeHuman installer is missing: $installer"
    }
    $install = Start-Process -FilePath $installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART') -Wait -PassThru
    if ($install.ExitCode -ne 0) {
        throw "MakeHuman installer exited with code $($install.ExitCode)."
    }
}

$pythonExe = Join-Path $MakeHumanRoot 'Python\python.exe'
$wrapper = Join-Path $MakeHumanRoot 'mhstartwrapper.py'
$pluginsRoot = Join-Path $MakeHumanRoot 'makehuman\plugins'
foreach ($required in @($pythonExe, $wrapper, $pluginsRoot, $pluginSource, $suitsRoot)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required MakeHuman pipeline path is missing: $required"
    }
}
Copy-Item -LiteralPath $pluginSource -Destination $pluginsRoot -Recurse -Force

$jobs = @(
    @{ Character = 'engineer'; Folder = 'Engineer'; File = 'WS_Engineer.fbx'; Destination = 'EngineerGuHeng' },
    @{ Character = 'doctor'; Folder = 'Doctor'; File = 'WS_Doctor.fbx'; Destination = 'DoctorYeCheng' }
)

try {
    $env:WS_MH_AUTOGEN = '1'
    $env:WS_MH_SUITS_ROOT = $suitsRoot
    foreach ($job in $jobs) {
        $temporaryFolder = Join-Path $cacheRoot (Join-Path 'output' $job.Folder)
        New-Item -ItemType Directory -Path $temporaryFolder -Force | Out-Null
        $outputFbx = Join-Path $temporaryFolder $job.File
        $env:WS_MH_CHARACTER = $job.Character
        $env:WS_MH_OUTPUT = $outputFbx
        Push-Location $MakeHumanRoot
        try {
            & $pythonExe $wrapper
            if ($LASTEXITCODE -ne 0) {
                throw "MakeHuman exited with code $LASTEXITCODE for $($job.Character)."
            }
        }
        finally {
            Pop-Location
        }
        if (-not (Test-Path -LiteralPath $outputFbx)) {
            throw "MakeHuman did not produce $outputFbx. Check Documents\makehuman\v1py3\makehuman.log."
        }
        $destination = Join-Path $workspaceRoot (Join-Path 'SourceAssets\MakeHuman\Characters' $job.Destination)
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Copy-Item -Path (Join-Path $temporaryFolder '*') -Destination $destination -Recurse -Force
        Write-Host "Generated $($job.Character): $outputFbx"
    }
}
finally {
    Remove-Item Env:WS_MH_AUTOGEN -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_CHARACTER -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_OUTPUT -ErrorAction SilentlyContinue
    Remove-Item Env:WS_MH_SUITS_ROOT -ErrorAction SilentlyContinue
}
